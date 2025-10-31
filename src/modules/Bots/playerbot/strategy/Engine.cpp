#include "../../botpch.h"
#include "../playerbot.h"

#include "Engine.h"
#include "../PlayerbotAIConfig.h"
#include <cstddef>

using namespace ai;
using namespace std;

Engine::Engine(PlayerbotAI* ai, AiObjectContext *factory) : PlayerbotAIAware(ai), aiObjectContext(factory)
{
    lastRelevance = 0.0f;
    testMode = false;
}

bool ActionExecutionListeners::Before(Action* action, Event event)
{
    bool result = true;
    for (auto* listener : listeners)
    {
        result &= listener->Before(action, event);
    }
    return result;
}

void ActionExecutionListeners::After(Action* action, bool executed, Event event)
{
    for (auto* listener : listeners)
    {
        listener->After(action, executed, event);
    }
}

bool ActionExecutionListeners::OverrideResult(Action* action, bool executed, Event event)
{
    bool result = executed;
    for (auto* listener : listeners)
    {
        result = listener->OverrideResult(action, result, event);
    }
    return result;
}

bool ActionExecutionListeners::AllowExecution(Action* action, Event event)
{
    bool result = true;
    for (auto* listener : listeners)
    {
        result &= listener->AllowExecution(action, event);
    }
    return result;
}

ActionExecutionListeners::~ActionExecutionListeners()
{
    for (auto* listener : listeners)
    {
        delete listener;
    }
    listeners.clear();
}


Engine::~Engine(void)
{
    Reset();

    strategies.clear();
}

void Engine::Reset()
{
    ActionNode* action = nullptr;
    do
    {
        action = queue.Pop();
        delete action;
    } while (action != nullptr);

    for (auto* trigger : triggers)
    {
        delete trigger;
    }
    triggers.clear();

    for (auto* multiplier : multipliers)
    {
        delete multiplier;
    }
    multipliers.clear();
}

void Engine::Init()
{
    Reset();

    for (auto& entry : strategies)
    {
        Strategy* strategy = entry.second;
        strategy->InitMultipliers(multipliers);
        strategy->InitTriggers(triggers);
        Event emptyEvent;
        MultiplyAndPush(strategy->getDefaultActions(), 0.0f, false, emptyEvent);
    }

    if (testMode)
    {
        FILE* file = fopen("test.log", "w");
        fprintf(file, "\n");
        fclose(file);
    }
}


bool Engine::DoNextAction(Unit* unit, int depth)
{
    LogAction("--- AI Tick ---");
    if (sPlayerbotAIConfig.logValuesPerTick)
    {
        LogValues();
    }

    bool actionExecuted = false;
    ActionBasket* basket = nullptr;

    time_t currentTime = time(0);
    aiObjectContext->Update();
    ProcessTriggers();

    std::size_t iterations = 0;
    const auto iterationsPerTick = queue.Size() * static_cast<std::size_t>(sPlayerbotAIConfig.iterationsPerTick);
    do {
        basket = queue.Peek();
        if (basket)
        {
            if (++iterations > iterationsPerTick)
            {
                break;
            }

            float relevance = basket->getRelevance(); // just for reference
            bool skipPrerequisites = basket->isSkipPrerequisites();
            Event event = basket->getEvent();
            // NOTE: queue.Pop() deletes basket
            ActionNode* actionNode = queue.Pop();
            Action* action = InitializeAction(actionNode);

            if (!action)
            {
                LogAction("A:%s - UNKNOWN", actionNode->getName().c_str());
            }
            else if (action->isUseful())
            {
                for (auto* multiplier : multipliers)
                {
                    relevance *= multiplier->GetValue(action);
                    if (!relevance)
                    {
                        LogAction("Multiplier %s made action %s useless", multiplier->getName().c_str(), action->getName().c_str());
                        break;
                    }
                }

                if (action->isPossible() && relevance)
                {
                    if ((!skipPrerequisites || lastRelevance-relevance > 0.04) &&
                            MultiplyAndPush(actionNode->getPrerequisites(), relevance + 0.02, false, event))
                    {
                        PushAgain(actionNode, relevance + 0.01, event);
                        continue;
                    }

                    actionExecuted = ListenAndExecute(action, event);

                    if (actionExecuted)
                    {
                        LogAction("A:%s - OK", action->getName().c_str());
                        MultiplyAndPush(actionNode->getContinuers(), 0, false, event);
                        lastRelevance = relevance;
                        delete actionNode;
                        break;
                    }
                    else
                    {
                        MultiplyAndPush(actionNode->getAlternatives(), relevance + 0.03, false, event);
                        LogAction("A:%s - FAILED", action->getName().c_str());
                    }
                }
                else
                {
                    MultiplyAndPush(actionNode->getAlternatives(), relevance + 0.03, false, event);
                    LogAction("A:%s - IMPOSSIBLE", action->getName().c_str());
                }
            }
            else
            {
                lastRelevance = relevance;
                LogAction("A:%s - USELESS", action->getName().c_str());
            }
            delete actionNode;
        }
    }
    while (basket);

    if (!basket)
    {
        lastRelevance = 0.0f;
        PushDefaultActions();
        if (queue.Peek() && depth < 2)
        {
            return DoNextAction(unit, depth + 1);
        }
    }

    if (time(0) - currentTime > 1) {
    {
        LogAction("too long execution");
    }
    }

    if (!actionExecuted)
    {
        LogAction("no actions executed");
    }

    return actionExecuted;
}

ActionNode* Engine::CreateActionNode(string name)
{
    for (auto& entry : strategies)
    {
        Strategy* strategy = entry.second;
        ActionNode* node = strategy->GetAction(name);
        if (node)
        {
            return node;
        }
    }
    return new ActionNode (name,
        /*P*/ nullptr,
        /*A*/ nullptr,
        /*C*/ nullptr);
}

bool Engine::MultiplyAndPush(NextAction** actions, float forceRelevance, bool skipPrerequisites, Event event)
{
    bool pushed = false;
    if (actions)
    {
        for (NextAction** next = actions; *next != nullptr; ++next)
        {
            NextAction* nextAction = *next;
            ActionNode* action = CreateActionNode(nextAction->getName());
            InitializeAction(action);

            float k = nextAction->getRelevance();
            if (forceRelevance > 0.0f)
            {
                k = forceRelevance;
            }

            if (k > 0)
            {
                LogAction("PUSH:%s %f", action->getName().c_str(), k);
                queue.Push(new ActionBasket(action, k, skipPrerequisites, event));
                pushed = true;
            }

            delete nextAction;
        }
        delete[] actions;
    }
    return pushed;
}

ActionResult Engine::ExecuteAction(string &name)
{
    bool result = false;

    ActionNode *actionNode = CreateActionNode(name);
    if (!actionNode)
    {
        return ACTION_RESULT_UNKNOWN;
    }

    Action* action = InitializeAction(actionNode);
    if (!action)
    {
        return ACTION_RESULT_UNKNOWN;
    }

    if (!action->isPossible())
    {
        delete actionNode;
        return ACTION_RESULT_IMPOSSIBLE;
    }

    if (!action->isUseful())
    {
        delete actionNode;
        return ACTION_RESULT_USELESS;
    }

    action->MakeVerbose();
    Event emptyEvent;
    result = ListenAndExecute(action, emptyEvent);
    MultiplyAndPush(action->getContinuers(), 0.0f, false, emptyEvent);
    delete actionNode;
    return result ? ACTION_RESULT_OK : ACTION_RESULT_FAILED;
}

void Engine::addStrategy(string name)
{
    removeStrategy(name);

    Strategy* strategy = aiObjectContext->GetStrategy(name);
    if (strategy)
    {
        set<string> siblings = aiObjectContext->GetSiblingStrategy(name);
        for (const auto& sibling : siblings)
        {
            removeStrategy(sibling);
        }

        LogAction("S:+%s", strategy->getName().c_str());
        strategies[strategy->getName()] = strategy;
    }
    Init();
}

void Engine::addStrategies(string first, ...)
{
    addStrategy(first);

    va_list vl;
    va_start(vl, first);

    const char* cur;
    do
    {
        cur = va_arg(vl, const char*);
        if (cur)
        {
            addStrategy(cur);
        }
    }
    while (cur);

    va_end(vl);
}

bool Engine::removeStrategy(string name)
{
    map<string, Strategy*>::iterator i = strategies.find(name);
    if (i == strategies.end())
    {
        return false;
    }

    LogAction("S:-%s", name.c_str());
    strategies.erase(i);
    Init();
    return true;
}

void Engine::removeAllStrategies()
{
    strategies.clear();
    Init();
}

void Engine::toggleStrategy(string name)
{
    if (!removeStrategy(name))
    {
        addStrategy(name);
    }
}

bool Engine::HasStrategy(string name)
{
    return strategies.find(name) != strategies.end();
}

void Engine::ProcessTriggers()
{
    for (auto* node : triggers)
    {
        if (!node)
        {
            continue;
        }

        Trigger* trigger = node->getTrigger();
        if (!trigger)
        {
            trigger = aiObjectContext->GetTrigger(node->getName());
            node->setTrigger(trigger);
        }

        if (!trigger)
        {
            continue;
        }

        if (testMode || trigger->needCheck())
        {
            Event event = trigger->Check();
            if (!event)
            {
                continue;
            }

            LogAction("T:%s", trigger->getName().c_str());
            MultiplyAndPush(node->getHandlers(), 0.0f, false, event);
        }
    }

    for (auto* triggerNode : triggers)
    {
        Trigger* trigger = triggerNode->getTrigger();
        if (trigger)
        {
            trigger->Reset();
        }
    }
}

void Engine::PushDefaultActions()
{
    for (auto& entry : strategies)
    {
        Strategy* strategy = entry.second;
        Event emptyEvent;
        MultiplyAndPush(strategy->getDefaultActions(), 0.0f, false, emptyEvent);
    }
}

string Engine::ListStrategies()
{
    string s = "Strategies: ";

    if (strategies.empty())
    {
        return s;
    }

    for (const auto& entry : strategies)
    {
        s.append(entry.first);
        s.append(", ");
    }
    return s.substr(0, s.length() - 2);
}

void Engine::PushAgain(ActionNode* actionNode, float relevance, Event event)
{
    NextAction** nextAction = new NextAction*[2];
    nextAction[0] = new NextAction(actionNode->getName(), relevance);
    nextAction[1] = nullptr;
    MultiplyAndPush(nextAction, relevance, true, event);
    delete actionNode;
}

bool Engine::ContainsStrategy(StrategyType type)
{
    for (const auto& entry : strategies)
    {
        Strategy* strategy = entry.second;
        if (strategy->GetType() & type)
        {
            return true;
        }
    }
    return false;
}

Action* Engine::InitializeAction(ActionNode* actionNode)
{
    Action* action = actionNode->getAction();
    if (!action)
    {
        action = aiObjectContext->GetAction(actionNode->getName());
        actionNode->setAction(action);
    }
    return action;
}

bool Engine::ListenAndExecute(Action* action, Event event)
{
    bool actionExecuted = false;

    if (actionExecutionListeners.Before(action, event))
    {
        actionExecuted = actionExecutionListeners.AllowExecution(action, event) ? action->Execute(event) : true;
    }

    actionExecuted = actionExecutionListeners.OverrideResult(action, actionExecuted, event);
    actionExecutionListeners.After(action, actionExecuted, event);
    return actionExecuted;
}

void Engine::LogAction(const char* format, ...)
{
    char buf[1024];

    va_list ap;
    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    lastAction = buf;

    if (testMode)
    {
        FILE* file = fopen("test.log", "a");
        fprintf(file, buf);
        fprintf(file, "\n");
        fclose(file);
    }
    else
    {
        Player* bot = ai->GetBot();
        if (sPlayerbotAIConfig.logInGroupOnly && !bot->GetGroup())
        {
            return;
        }

        sLog.outDebug("%s %s", bot->GetName(), buf);
    }
}

void Engine::ChangeStrategy(string &names)
{
    vector<string> splitted = split(names, ',');
    for (vector<string>::iterator i = splitted.begin(); i != splitted.end(); i++)
    {
        const char* name = i->c_str();
        switch (name[0])
        {
        case '+':
            addStrategy(name+1);
            break;
        case '-':
            removeStrategy(name+1);
            break;
        case '~':
            toggleStrategy(name+1);
            break;
        case '?':
            ai->TellMaster(ListStrategies());
            break;
        }
    }
}

void Engine::LogValues()
{
    if (testMode)
    {
        return;
    }

    Player* bot = ai->GetBot();
    if (sPlayerbotAIConfig.logInGroupOnly && !bot->GetGroup())
    {
        return;
    }

    string text = ai->GetAiObjectContext()->FormatValues();
    sLog.outDebug( "Values for %s: %s", bot->GetName(), text.c_str());
}
