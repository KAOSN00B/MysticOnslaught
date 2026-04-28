#pragma once
#include <vector>
#include <memory>
#include <functional>

// Lightweight stateless Behaviour Tree used by the Molarbeast boss AI.
// Every node is re-evaluated top-down each frame — no history or memory.

enum class BTStatus { RUNNING, SUCCESS, FAILURE };

// ── Base node ────────────────────────────────────────────────────────────────
class BTNode
{
public:
    virtual ~BTNode() = default;
    virtual BTStatus Tick(float dt) = 0;
    virtual void     AddChild(std::unique_ptr<BTNode>) {}
};

// ── Selector — OR node ───────────────────────────────────────────────────────
// Tries children left-to-right. Returns the first non-FAILURE result.
// Returns FAILURE only when every child fails.
class BTSelector : public BTNode
{
public:
    void AddChild(std::unique_ptr<BTNode> child) override
    { _children.push_back(std::move(child)); }

    BTStatus Tick(float dt) override
    {
        for (auto& child : _children)
        {
            BTStatus s = child->Tick(dt);
            if (s != BTStatus::FAILURE) return s;
        }
        return BTStatus::FAILURE;
    }

private:
    std::vector<std::unique_ptr<BTNode>> _children;
};

// ── Sequence — AND node ───────────────────────────────────────────────────────
// Runs children left-to-right. Fails fast on the first FAILURE.
// Returns SUCCESS only when every child succeeds.
class BTSequence : public BTNode
{
public:
    void AddChild(std::unique_ptr<BTNode> child) override
    { _children.push_back(std::move(child)); }

    BTStatus Tick(float dt) override
    {
        for (auto& child : _children)
        {
            BTStatus s = child->Tick(dt);
            if (s != BTStatus::SUCCESS) return s;
        }
        return BTStatus::SUCCESS;
    }

private:
    std::vector<std::unique_ptr<BTNode>> _children;
};

// ── Condition leaf ───────────────────────────────────────────────────────────
// Wraps a predicate: SUCCESS when true, FAILURE when false. No dt.
class BTCondition : public BTNode
{
public:
    explicit BTCondition(std::function<bool()> pred) : _pred(std::move(pred)) {}
    BTStatus Tick(float) override
    { return _pred() ? BTStatus::SUCCESS : BTStatus::FAILURE; }

private:
    std::function<bool()> _pred;
};

// ── Action leaf ───────────────────────────────────────────────────────────────
// Wraps a function returning BTStatus so the boss methods can be called
// directly. Can return RUNNING, SUCCESS, or FAILURE.
class BTAction : public BTNode
{
public:
    explicit BTAction(std::function<BTStatus(float)> fn) : _fn(std::move(fn)) {}
    BTStatus Tick(float dt) override { return _fn(dt); }

private:
    std::function<BTStatus(float)> _fn;
};
