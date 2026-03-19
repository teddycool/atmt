# Guardrails

These rules always apply. If any instruction conflicts with these, stop and ask the user.

## Mandatory Preflight Gate
Before starting any non-trivial task the agent MUST:
1. Read `AGENTS.md` to confirm active truck model and workflow rules.
2. Read `.context/TASK.md` to confirm the active task.
3. Read `.context/STATE.md` to recover current context.
4. Read `.context/PLAN.md` to know where to continue.
5. Write a plan to `.context/PLAN.md` before doing any implementation work.

## Safety Rules
- Never delete files without explicit user confirmation.
- Never push to remote git without explicit user instruction.
- Never modify hardware configuration values (broker IP, PWM limits, sensor thresholds) without explicit user instruction.
- Never commit credentials, IP addresses, or hardware-specific secrets to version control without user approval.

## Code Rules
- Prefer the minimum design that satisfies the current need.
- Do not add features, generalisations, or flexibility beyond what is asked. Propose extras as explicit user decisions.
- Comments must explain *why*, not restate what the code does line by line.
- Whenever a shared variable is added, removed, renamed, or has its unit changed, update `docs/VARIABLE_MODEL.md` in the same work item.

## Context Hygiene
- `STATE.md` is live working memory for the current task only — keep it short.
- `PLAN.md` is the execution plan — update it at every meaningful step.
- `PROJECT_HISTORY.md` is append-only — do not edit past entries.
- `TASK.md` is the task definition — do not update it during execution unless the task itself changes.
- Log every file addition or deletion in `PROJECT_HISTORY.md`.

## Conflict Handling
- If instructions conflict, append the conflict to `CONFLICTS.md` in table format, stop, and wait for user resolution.
