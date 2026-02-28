#include "fabric/core/BTDebugPanel.hh"

#include "fabric/core/BehaviorAI.hh"
#include "fabric/core/Log.hh"
#include <behaviortree_cpp/loggers/bt_observer.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <sstream>

namespace fabric {

namespace {

const char* statusToString(BT::NodeStatus s) {
    switch (s) {
        case BT::NodeStatus::SUCCESS:
            return "SUCCESS";
        case BT::NodeStatus::FAILURE:
            return "FAILURE";
        case BT::NodeStatus::RUNNING:
            return "RUNNING";
        case BT::NodeStatus::SKIPPED:
            return "SKIPPED";
        case BT::NodeStatus::IDLE:
        default:
            return "IDLE";
    }
}

void collectNodeInfo(const BT::TreeObserver& observer, const BT::Tree& /*tree*/, std::vector<BTNodeInfo>& out) {
    // Use observer's UID-to-path map (sorted by UID, which reflects tree order).
    // Compute depth from path separator count.
    const auto& uidToPath = observer.uidToPath();
    const auto& stats = observer.statistics();

    for (const auto& [uid, path] : uidToPath) {
        BTNodeInfo info;

        // Extract leaf name from path (after last '/')
        auto lastSlash = path.rfind('/');
        info.name = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

        // Depth from separator count
        info.depth = 0;
        for (char c : path) {
            if (c == '/') {
                info.depth++;
            }
        }

        // Status from observer statistics
        auto sit = stats.find(uid);
        if (sit != stats.end()) {
            info.status = statusToString(sit->second.current_status);
        } else {
            info.status = "IDLE";
        }

        out.push_back(std::move(info));
    }
}

} // namespace

void BTDebugPanel::init(Rml::Context* context) {
    if (!context) {
        FABRIC_LOG_ERROR("BTDebugPanel::init called with null context");
        return;
    }

    context_ = context;

    Rml::DataModelConstructor constructor = context_->CreateDataModel("bt_debug");
    if (!constructor) {
        FABRIC_LOG_ERROR("BTDebugPanel: failed to create data model");
        return;
    }

    constructor.Bind("selected_npc", &selectedNpcName_);
    constructor.Bind("node_list", &nodeListText_);

    modelHandle_ = constructor.GetModelHandle();

    document_ = context_->LoadDocument("assets/ui/bt_debug.rml");
    if (document_) {
        document_->Hide();
        FABRIC_LOG_INFO("BTDebugPanel document loaded");
    } else {
        FABRIC_LOG_WARN("BTDebugPanel: failed to load bt_debug.rml");
    }

    initialized_ = true;
}

void BTDebugPanel::update(BehaviorAI& ai, flecs::entity selectedNpc) {
    if (!initialized_ || !visible_)
        return;

    // Use the passed-in entity or fall back to internal selection
    flecs::entity npc = selectedNpc.is_valid() ? selectedNpc : selectedNpc_;
    if (!npc.is_valid() || !npc.is_alive()) {
        selectedNpcName_ = "(none)";
        nodeListText_ = "";
        if (modelHandle_) {
            modelHandle_.DirtyAllVariables();
        }
        return;
    }

    // Update NPC name
    const char* entityName = npc.name().c_str();
    if (entityName && entityName[0] != '\0') {
        selectedNpcName_ = entityName;
    } else {
        selectedNpcName_ = "NPC #" + std::to_string(npc.id());
    }

    // Get observer and build node list
    auto* observer = ai.observerFor(npc);
    if (!observer) {
        nodeListText_ = "(no observer)";
        if (modelHandle_) {
            modelHandle_.DirtyAllVariables();
        }
        return;
    }

    // Collect node info and check entity has a tree
    if (!npc.has<BehaviorTreeComponent>()) {
        nodeListText_ = "(no tree)";
        if (modelHandle_) {
            modelHandle_.DirtyAllVariables();
        }
        return;
    }

    const auto& btc = npc.get<BehaviorTreeComponent>();
    std::vector<BTNodeInfo> nodes;
    collectNodeInfo(*observer, btc.tree, nodes);

    // Build formatted text
    std::ostringstream oss;
    for (const auto& node : nodes) {
        for (int i = 0; i < node.depth; ++i) {
            oss << "  ";
        }
        oss << "[" << node.status << "] " << node.name << "\n";
    }
    nodeListText_ = oss.str();

    if (modelHandle_) {
        modelHandle_.DirtyAllVariables();
    }
}

void BTDebugPanel::toggle() {
    visible_ = !visible_;
    if (document_) {
        if (visible_) {
            document_->Show();
        } else {
            document_->Hide();
        }
    }
}

void BTDebugPanel::shutdown() {
    if (document_) {
        document_->Close();
        document_ = nullptr;
    }
    if (context_ && modelHandle_) {
        context_->RemoveDataModel("bt_debug");
    }
    modelHandle_ = {};
    initialized_ = false;
    visible_ = false;
    context_ = nullptr;
}

bool BTDebugPanel::isVisible() const {
    return visible_;
}

void BTDebugPanel::selectNextNPC(BehaviorAI& ai, flecs::world& world) {
    (void)ai; // Reserved for future observer-aware filtering
    // Gather all entities with BehaviorTreeComponent
    std::vector<flecs::entity> npcs;
    auto q = world.query_builder<const BehaviorTreeComponent>().build();
    q.each([&npcs](flecs::entity e, const BehaviorTreeComponent&) { npcs.push_back(e); });

    if (npcs.empty()) {
        selectedNpc_ = flecs::entity();
        FABRIC_LOG_INFO("BT Debug: no NPCs with behavior trees");
        return;
    }

    // Find current index and advance
    size_t currentIdx = 0;
    bool found = false;
    for (size_t i = 0; i < npcs.size(); ++i) {
        if (npcs[i] == selectedNpc_) {
            currentIdx = i;
            found = true;
            break;
        }
    }

    size_t nextIdx = found ? (currentIdx + 1) % npcs.size() : 0;
    selectedNpc_ = npcs[nextIdx];

    FABRIC_LOG_INFO("BT Debug: selected NPC #{}", selectedNpc_.id());
}

flecs::entity BTDebugPanel::selectedNpc() const {
    return selectedNpc_;
}

} // namespace fabric
