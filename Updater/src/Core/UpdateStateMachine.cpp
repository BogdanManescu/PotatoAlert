// Copyright 2024 <github.com/razaqq>

#include "Updater/Core/UpdateStateMachine.hpp"

using namespace PotatoAlert::Updater;

UpdateStateMachine::UpdateStateMachine(UpdateState initialState)
    : m_currentState(initialState)
    , m_initialState(initialState)
{
    InitializeValidTransitions();
}

UpdateState UpdateStateMachine::GetCurrentState() const
{
    return m_currentState;
}

UpdateResult UpdateStateMachine::TransitionTo(UpdateState newState)
{
    if (!CanTransitionTo(newState))
    {
        return UpdateResult::InvalidConfiguration;
    }
    
    UpdateState oldState = m_currentState;
    m_currentState = newState;
    
    NotifyTransition(oldState, newState);
    
    return UpdateResult::Success;
}

bool UpdateStateMachine::CanTransitionTo(UpdateState newState) const
{
    return IsValidTransition(m_currentState, newState);
}

std::vector<UpdateState> UpdateStateMachine::GetValidTransitions() const
{
    auto it = m_validTransitions.find(m_currentState);
    if (it != m_validTransitions.end())
    {
        return it->second;
    }
    return {};
}

void UpdateStateMachine::SetStateHandler(UpdateState state, StateHandler handler)
{
    m_stateHandlers[state] = std::move(handler);
}

void UpdateStateMachine::SetTransitionCallback(StateCallback callback)
{
    m_transitionCallback = std::move(callback);
}

UpdateResult UpdateStateMachine::ExecuteCurrentState()
{
    auto it = m_stateHandlers.find(m_currentState);
    if (it != m_stateHandlers.end())
    {
        return it->second();
    }
    
    return UpdateResult::Success; // No handler is okay for some states
}

bool UpdateStateMachine::IsValidTransition(UpdateState from, UpdateState to) const
{
    auto it = m_validTransitions.find(from);
    if (it == m_validTransitions.end())
        return false;
    
    const auto& validStates = it->second;
    return std::find(validStates.begin(), validStates.end(), to) != validStates.end();
}

std::string UpdateStateMachine::GetStateName(UpdateState state) const
{
    switch (state)
    {
        case UpdateState::Idle: return "Idle";
        case UpdateState::CheckingForUpdates: return "Checking for Updates";
        case UpdateState::UpdateAvailable: return "Update Available";
        case UpdateState::Downloading: return "Downloading";
        case UpdateState::Verifying: return "Verifying";
        case UpdateState::BackingUp: return "Creating Backup";
        case UpdateState::Installing: return "Installing";
        case UpdateState::Finalizing: return "Finalizing";
        case UpdateState::RollingBack: return "Rolling Back";
        case UpdateState::Complete: return "Complete";
        case UpdateState::Failed: return "Failed";
        default: return "Unknown";
    }
}

void UpdateStateMachine::Reset()
{
    m_currentState = m_initialState;
}

void UpdateStateMachine::InitializeValidTransitions()
{
    // Define valid state transitions
    m_validTransitions[UpdateState::Idle] = {
        UpdateState::CheckingForUpdates
    };
    
    m_validTransitions[UpdateState::CheckingForUpdates] = {
        UpdateState::UpdateAvailable,
        UpdateState::Complete,  // No update needed
        UpdateState::Failed
    };
    
    m_validTransitions[UpdateState::UpdateAvailable] = {
        UpdateState::Downloading,
        UpdateState::Idle,      // User decides not to update
        UpdateState::Failed
    };
    
    m_validTransitions[UpdateState::Downloading] = {
        UpdateState::Verifying,
        UpdateState::Failed
    };
    
    m_validTransitions[UpdateState::Verifying] = {
        UpdateState::BackingUp,
        UpdateState::Failed
    };
    
    m_validTransitions[UpdateState::BackingUp] = {
        UpdateState::Installing,
        UpdateState::Failed
    };
    
    m_validTransitions[UpdateState::Installing] = {
        UpdateState::Finalizing,
        UpdateState::RollingBack  // Installation failed
    };
    
    m_validTransitions[UpdateState::Finalizing] = {
        UpdateState::Complete,
        UpdateState::Failed
    };
    
    m_validTransitions[UpdateState::RollingBack] = {
        UpdateState::Failed,    // Rollback completed (still a failure state)
        UpdateState::Complete   // Rollback successful, system restored
    };
    
    m_validTransitions[UpdateState::Complete] = {
        UpdateState::Idle       // Ready for next update
    };
    
    m_validTransitions[UpdateState::Failed] = {
        UpdateState::Idle,      // Reset to try again
        UpdateState::RollingBack // Attempt recovery
    };
}

void UpdateStateMachine::NotifyTransition(UpdateState oldState, UpdateState newState)
{
    if (m_transitionCallback)
    {
        m_transitionCallback(oldState, newState);
    }
}
