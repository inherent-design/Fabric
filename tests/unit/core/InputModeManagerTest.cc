#include "fabric/core/InputModeManager.hh"
#include "fabric/utils/ErrorHandling.hh"
#include <gtest/gtest.h>

using namespace fabric;

class AppModeManagerTest : public ::testing::Test {
  protected:
    void SetUp() override { mgr = std::make_unique<AppModeManager>(); }

    std::unique_ptr<AppModeManager> mgr;
};

TEST_F(AppModeManagerTest, InitialModeIsGame) {
    EXPECT_EQ(mgr->current(), AppMode::Game);
    EXPECT_EQ(mgr->previous(), AppMode::Game);
}

TEST_F(AppModeManagerTest, TransitionGameToPaused) {
    mgr->transition(AppMode::Paused);
    EXPECT_EQ(mgr->current(), AppMode::Paused);
    EXPECT_EQ(mgr->previous(), AppMode::Game);
}

TEST_F(AppModeManagerTest, TransitionGameToConsole) {
    mgr->transition(AppMode::Console);
    EXPECT_EQ(mgr->current(), AppMode::Console);
    EXPECT_EQ(mgr->previous(), AppMode::Game);
}

TEST_F(AppModeManagerTest, TransitionGameToEditor) {
    mgr->transition(AppMode::Editor);
    EXPECT_EQ(mgr->current(), AppMode::Editor);
    EXPECT_EQ(mgr->previous(), AppMode::Game);
}

TEST_F(AppModeManagerTest, TransitionGameToMenu) {
    mgr->transition(AppMode::Menu);
    EXPECT_EQ(mgr->current(), AppMode::Menu);
    EXPECT_EQ(mgr->previous(), AppMode::Game);
}

TEST_F(AppModeManagerTest, TransitionBackToGame) {
    mgr->transition(AppMode::Console);
    mgr->transition(AppMode::Game);
    EXPECT_EQ(mgr->current(), AppMode::Game);
    EXPECT_EQ(mgr->previous(), AppMode::Console);
}

TEST_F(AppModeManagerTest, InvalidTransitionThrows) {
    mgr->transition(AppMode::Console);
    EXPECT_THROW(mgr->transition(AppMode::Editor), FabricException);
    EXPECT_EQ(mgr->current(), AppMode::Console);
}

TEST_F(AppModeManagerTest, InvalidTransitionPausedToConsole) {
    mgr->transition(AppMode::Paused);
    EXPECT_THROW(mgr->transition(AppMode::Console), FabricException);
}

TEST_F(AppModeManagerTest, InvalidTransitionEditorToMenu) {
    mgr->transition(AppMode::Editor);
    EXPECT_THROW(mgr->transition(AppMode::Menu), FabricException);
}

TEST_F(AppModeManagerTest, SelfTransitionIsNoop) {
    mgr->transition(AppMode::Game);
    EXPECT_EQ(mgr->current(), AppMode::Game);
    EXPECT_EQ(mgr->previous(), AppMode::Game);
}

TEST_F(AppModeManagerTest, TogglePauseFromGame) {
    mgr->togglePause();
    EXPECT_EQ(mgr->current(), AppMode::Paused);
    EXPECT_EQ(mgr->previous(), AppMode::Game);
}

TEST_F(AppModeManagerTest, TogglePauseFromPaused) {
    mgr->togglePause();
    mgr->togglePause();
    EXPECT_EQ(mgr->current(), AppMode::Game);
    EXPECT_EQ(mgr->previous(), AppMode::Paused);
}

TEST_F(AppModeManagerTest, TogglePauseFromConsoleIsNoop) {
    mgr->transition(AppMode::Console);
    mgr->togglePause();
    EXPECT_EQ(mgr->current(), AppMode::Console);
}

TEST_F(AppModeManagerTest, ModeFlagsGame) {
    auto f = AppModeManager::flags(AppMode::Game);
    EXPECT_TRUE(f.captureMouse);
    EXPECT_FALSE(f.pauseSimulation);
    EXPECT_FALSE(f.routeToUI);
    EXPECT_TRUE(f.routeToGame);
}

TEST_F(AppModeManagerTest, ModeFlagsPaused) {
    auto f = AppModeManager::flags(AppMode::Paused);
    EXPECT_FALSE(f.captureMouse);
    EXPECT_TRUE(f.pauseSimulation);
    EXPECT_TRUE(f.routeToUI);
    EXPECT_FALSE(f.routeToGame);
}

TEST_F(AppModeManagerTest, ModeFlagsConsole) {
    auto f = AppModeManager::flags(AppMode::Console);
    EXPECT_FALSE(f.captureMouse);
    EXPECT_FALSE(f.pauseSimulation);
    EXPECT_TRUE(f.routeToUI);
    EXPECT_TRUE(f.routeToGame);
}

TEST_F(AppModeManagerTest, ModeFlagsMenu) {
    auto f = AppModeManager::flags(AppMode::Menu);
    EXPECT_FALSE(f.captureMouse);
    EXPECT_TRUE(f.pauseSimulation);
    EXPECT_TRUE(f.routeToUI);
    EXPECT_FALSE(f.routeToGame);
}

TEST_F(AppModeManagerTest, ModeFlagsEditor) {
    auto f = AppModeManager::flags(AppMode::Editor);
    EXPECT_FALSE(f.captureMouse);
    EXPECT_TRUE(f.pauseSimulation);
    EXPECT_TRUE(f.routeToUI);
    EXPECT_FALSE(f.routeToGame);
}

TEST_F(AppModeManagerTest, ObserverFiresOnTransition) {
    AppMode observedFrom = AppMode::Game;
    AppMode observedTo = AppMode::Game;
    int callCount = 0;

    mgr->addObserver([&](AppMode from, AppMode to) {
        observedFrom = from;
        observedTo = to;
        ++callCount;
    });

    mgr->transition(AppMode::Paused);
    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(observedFrom, AppMode::Game);
    EXPECT_EQ(observedTo, AppMode::Paused);
}

TEST_F(AppModeManagerTest, ObserverNotCalledOnSelfTransition) {
    int callCount = 0;
    mgr->addObserver([&](AppMode, AppMode) { ++callCount; });
    mgr->transition(AppMode::Game);
    EXPECT_EQ(callCount, 0);
}

TEST_F(AppModeManagerTest, RemoveObserver) {
    int callCount = 0;
    auto id = mgr->addObserver([&](AppMode, AppMode) { ++callCount; });

    mgr->transition(AppMode::Paused);
    EXPECT_EQ(callCount, 1);

    EXPECT_TRUE(mgr->removeObserver(id));
    mgr->transition(AppMode::Game);
    EXPECT_EQ(callCount, 1);
}

TEST_F(AppModeManagerTest, RemoveNonexistentObserver) {
    EXPECT_FALSE(mgr->removeObserver("nonexistent"));
}

TEST_F(AppModeManagerTest, NullObserverThrows) {
    EXPECT_THROW(mgr->addObserver(nullptr), FabricException);
}

TEST_F(AppModeManagerTest, MultipleObservers) {
    int count1 = 0;
    int count2 = 0;
    mgr->addObserver([&](AppMode, AppMode) { ++count1; });
    mgr->addObserver([&](AppMode, AppMode) { ++count2; });

    mgr->transition(AppMode::Paused);
    EXPECT_EQ(count1, 1);
    EXPECT_EQ(count2, 1);
}

TEST_F(AppModeManagerTest, PreviousTracksLastMode) {
    mgr->transition(AppMode::Console);
    mgr->transition(AppMode::Game);
    mgr->transition(AppMode::Editor);
    EXPECT_EQ(mgr->previous(), AppMode::Game);
    EXPECT_EQ(mgr->current(), AppMode::Editor);
}

TEST_F(AppModeManagerTest, IsValidTransitionSelfIsValid) {
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Game, AppMode::Game));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Paused, AppMode::Paused));
}

TEST_F(AppModeManagerTest, IsValidTransitionTable) {
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Game, AppMode::Paused));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Paused, AppMode::Game));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Game, AppMode::Console));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Console, AppMode::Game));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Game, AppMode::Editor));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Editor, AppMode::Game));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Game, AppMode::Menu));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Menu, AppMode::Game));

    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Console, AppMode::Paused));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Editor, AppMode::Paused));
    EXPECT_TRUE(AppModeManager::isValidTransition(AppMode::Menu, AppMode::Paused));
}

TEST_F(AppModeManagerTest, IsValidTransitionInvalid) {
    EXPECT_FALSE(AppModeManager::isValidTransition(AppMode::Paused, AppMode::Console));
    EXPECT_FALSE(AppModeManager::isValidTransition(AppMode::Console, AppMode::Editor));
    EXPECT_FALSE(AppModeManager::isValidTransition(AppMode::Editor, AppMode::Menu));
    EXPECT_FALSE(AppModeManager::isValidTransition(AppMode::Menu, AppMode::Console));
}

TEST_F(AppModeManagerTest, AppModeToString) {
    EXPECT_EQ(appModeToString(AppMode::Game), "Game");
    EXPECT_EQ(appModeToString(AppMode::Paused), "Paused");
    EXPECT_EQ(appModeToString(AppMode::Console), "Console");
    EXPECT_EQ(appModeToString(AppMode::Menu), "Menu");
    EXPECT_EQ(appModeToString(AppMode::Editor), "Editor");
    EXPECT_EQ(appModeToString(static_cast<AppMode>(99)), "Unknown");
}
