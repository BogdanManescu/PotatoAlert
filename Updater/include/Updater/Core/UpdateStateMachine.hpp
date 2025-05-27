// Copyright 2024 <github.com/razaqq>
#pragma once

#include "Updater/Models/UpdateResult.hpp"

#include <functional>
#include <string>
#include <unordered_map>


namespace PotatoAlert::Updater {

class UpdateStateMachine
{
public:
    using StateHandler = std::function<UpdateResult()>;
    using StateCallback = std::function<void(UpdateState, UpdateState)>; // old, new

    UpdateStateMachine(UpdateState initialState = UpdateState::Idle);

    // State management
    UpdateState GetCurrentState() const;
    UpdateResult TransitionTo(UpdateState newState);
    bool CanTransitionTo(UpdateState newState) const;
    std::vector<UpdateState> GetValidTransitions() const;

    // State handlers
    void SetStateHandler(UpdateState state, StateHandler handler);
    void SetTransitionCallback(StateCallback callback);

    // Execute current state handler
    UpdateResult ExecuteCurrentState();

    // Validation
    bool IsValidTransition(UpdateState from, UpdateState to) const;
    std::string GetStateName(UpdateState state) const;

    // Reset to initial state
    void Reset();

private:
    void InitializeValidTransitions();
    void NotifyTransition(UpdateState oldState, UpdateState newState);

    UpdateState m_currentState;
    UpdateState m_initialState;
    
    std::unordered_map<UpdateState, std::vector<UpdateState>> m_validTransitions;
    std::unordered_map<UpdateState, StateHandler> m_stateHandlers;
    StateCallback m_transitionCallback;
};

}  // namespace PotatoAlert::Updater
