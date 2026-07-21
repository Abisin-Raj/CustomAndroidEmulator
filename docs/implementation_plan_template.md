# Implementation Plan Template

> Copy this file and fill in each section for every new implementation task.
> No code should be written until the plan is reviewed and approved.
> No task is complete until every Completion Criterion is checked.

---

## 1. Objective

_One paragraph describing the goal, the specific compatibility problem being
solved, and the intended outcome. Avoid implementation details here._

---

## 2. Current State

_A table or bullet list showing what is already built, what is partially done,
and what is missing. Use ✅ / ⚠️ / ❌ status markers._

| Component | Status | Notes |
|---|---|---|
| Example module | ✅ Done | Tested, 97/97 pass |
| Example stub | ⚠️ Partial | Compiles, not validated live |
| Missing artifact | ❌ Missing | Blocks Steps 2+ |

---

## 3. Prerequisites

_Conditions that must be true before any step can begin.
If a prerequisite requires user action (e.g. providing a file), say so explicitly.
For each prerequisite, document what to do if it cannot be met
(use a fallback / stub / skip)._

- **PRE-1:** ...
- **PRE-2:** ...

---

## 4. Open Questions

> [!IMPORTANT]
> List any decisions that are unresolved and would materially change the
> implementation. Answer these before starting. If the question cannot be
> answered, document the assumption made and its risk.

- **Q1:** ...
- **Q2:** ...

---

## 5. Proposed Steps

_Each step must have its own success criterion. Do not start the next step
until the previous one passes its criterion._

### Step N — [Name]

**What to do:**
- Bullet list of concrete actions

**Example Expected Output / Behavior:**
```
# example log excerpt or behavior description — not a specification
```

**Step N Success Criterion:**
_One sentence stating what "done" means for this specific step._

---

## 6. What NOT To Do

_Explicit constraints that prevent scope creep. List anything tempting
but out of scope for this task._

- ❌ Do not ...
- ❌ Do not ...

---

## 7. Rollback Plan

_If this step causes regressions or blocks guest execution, describe exactly
how to undo it safely._

- Disable the new feature via flag / config
- Restore the previous QEMU launch path
- Preserve all diagnostic logs before rollback
- Compare execution traces to isolate the regression

---

## 8. Verification Plan

### Automated
- Command to run: `...`
- Expected result: all N tests pass

### Manual / Live
- Steps to perform the live validation
- What to capture (log files, register dumps, screenshots)
- Where to save captured evidence

---

## 9. Completion Criteria

_This step is complete only when every item below is checked.
If any item cannot be checked, the step is not complete._

- [ ] ...
- [ ] ...
- [ ] Documentation updated
- [ ] Git commit made with evidence-linked message

---

_Template version: 2026-07-15. Update this version line if the template changes._
