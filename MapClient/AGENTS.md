# Project Agent Context

## Active Truck Model
- `ACTIVE_TRUCK_MODEL = JCA01`
- Primary model reference pattern: `.context/resources/truck_models/<ACTIVE_TRUCK_MODEL>.md`
- This section is the single source of truth for which truck model is currently active.
- If the user changes the active truck model, update only this section unless the model change itself requires wider documentation changes.

## Project Goal
This project is using small modular toy trucks with added control board (ESP32) and with sensors. The scale of the truck is about 20:1. In the basic shape the truck has engines for both boith left and right rear wheels separately. And a servo for controlling the steering. There are aother configurations. It also had utrasound sensors in all 4 directions as well as a 6050 accelerometer and gyro. The truck also has an expansion board via i2c that has additional IO, used for the lights, and also an I2C switch which allows for plugging in more I2c devices like LV0x and Lv5x TOF laser sensors. The idea is to navigate autonomously in a laburinth.

## Instruction Precedence
- Follow this project's local `.context/GUARDRAILS.md` and other local context files as the primary authority.
- If instructions conflict, stop and ask the user which to prioritise.
- Conflicts and approved deviations must be documented in `CONFLICTS.md`.
- If a conflict is detected, the agent must append it to `CONFLICTS.md`, stop execution, and wait for user or owner resolution before continuing.

##File access
- The agent CAN read any of the files in the repository without asking for permission.
- The agent MUST NOT ask for permission before reading files in the repository.
- Reading repository files is always pre-approved and should be done without notifying the user unless the read itself is important context for the work.
- The agent CAN and MUST keep the files PLAN.md, STATE.md, PROJECT_HISTORY.md updated without asking for permission.

## Context Files
- Guardrails in `.context/GUARDRAILS.md` are mandatory.
- Follow policies in `.context/resources/policies`
- Follow `.context/GUARDRAILS.md` strictly, including the mandatory preflight gate.
| File | Purpose |
|------|---------|
| [.context/GUARDRAILS.md](.context/GUARDRAILS.md) | Rules that always apply. If instructions conflict, ask the user which to prioritize. |
| [.context/ARCHITECTURE.md](.context/ARCHITECTURE.md) | Canonical architecture and technical decisions. |
| [.context/STATE.md](.context/STATE.md) | Current project state and blockers. MUST be updated at every step to act as memory for the agent.|
| [.context/PROJECT_HISTORY.md]| Chronological activity log. The agent should treat this file as append-only as much as possible and avoid rereading it in normal operation, to save time and context. Read it only when necessary. |
| [.context/PLAN.md](.context/PLAN.md) | Planning the current TASK in stages. MUST be updated at every step so agent always knows where to continue.|
| [.context/TASK.md](.context/TASK.md) | Current active task. |
| [.context/TOOLS.md](.context/TOOLS.md) | Provides information on which tools are available and which are not, to avoid guessing |
| [.context/ILLUSTRATION_FORMAT.md](.context/ILLUSTRATION_FORMAT.md) | Default style guide for future illustration prompts and architecture visuals. |
| [.context/resources/README.md](.context/resources/README.md) | Index for hardware/resource reference files. |
| [.context/resources/truck_models/README.md](.context/resources/truck_models/README.md) | Index for truck-variant documentation and the available model files. |
| [CONFLICTS.md](CONFLICTS.md) | A file containing all identified conflicts between instructions, rules and policies. |
| [BACKLOG.md](BACKLOG.md) | Where we put anything we figure out we need to do, but it is not part of the current TASK. |
| [DECISIONS.md](DECISIONS.md) | Significant project decisions must be logged here with date and username. Context-architecture housekeeping and simple backlog deferrals do not belong here. |

## Optional Context Indexes (Enable As Needed)
<!-- - [.context/skills/README.md](.context/skills/README.md) | Index for project skills/guides. | -->
<!-- - [.context/tools/README.md](.context/tools/README.md) | Index for tooling notes/runbooks. | -->
<!-- - [.context/decisions/README.md](.context/decisions/README.md) | Index for ADRs/decision records. | -->
<!-- - [.context/apis/README.md](.context/apis/README.md) | Index for external API contracts/examples. | -->

## Context Management
- Always log major changes/understandings in `.context/PROJECT_HISTORY.md`.
- Keep `.context/STATE.md` current as working memory for the current task only.
- Keep `.context/PLAN.md` plan for achiving the task and info so that another agent can resume from it without re-discovery. Has a completed, work in progress, and remaining section. COntinously updated by the agent.
- Keep `.context/TASK.md` aligned with the real active task definition only. Do not update until TASK has be finialised or cancelled. 
- Update `.context/ARCHITECTURE.md` whenever architecture/buffer-model/state-machine decisions change.
- Keep the active truck model declaration in this file accurate and visible, and keep the truck-model notes under `.context/resources/truck_models/` aligned with real hardware understanding.
- If anything in resources appears incorrect, ask the user before changing it.

## Standing permissions 
- The agent may update `.context/STATE.md` and append to `.context/PROJECT_HISTORY.md` after meaningful work without asking each time.

## Key Files
| File | Purpose |
|------|---------|
| `src/main.cpp` | Main application entry point. |
| `platformio.ini` | Build/upload/monitor configuration. |

## Maintenance Instructions
- `STATE.md`, `PLAN.md`, and `TASK.md` are mandatory working memory, not optional cleanup. The agent MUST update them during the work, not later, so they remain accurate after every meaningful step.
- Before substantial implementation work, the agent MUST verify that the active truck model declared above matches any model-specific assumptions in code, config, docs, and test setup. If the task targets a different physical truck, the agent MUST update the active model declaration or ask the user to resolve the mismatch before continuing.
- The agent MUST NOT leave code, docs, or configuration changes in place while `STATE.md`, `PLAN.md`, or `TASK.md` still describe an older state.
- For every non-trivial task, the agent MUST first turn the task into a step-by-step execution plan in `.context/PLAN.md` before doing substantial implementation work.
- The agent MUST then work from that plan rather than improvising from memory alone.
- When a plan step is completed, the agent MUST mark it as completed in `.context/PLAN.md`. When the active subtask changes, `.context/PLAN.md` MUST show that change clearly.
- Update `.context/STATE.md` as current-task memory only. Do not use it for history, and do not put next steps there if they belong in `.context/PLAN.md` or `.context/TASK.md`.
- `.context/STATE.md` MUST stay short and focused. It should contain only live facts needed to continue the current task after context loss or restart. If old items are no longer needed for immediate continuation, the agent MUST remove or compress them instead of letting the file grow indefinitely.
- Historical detail, completed-step narrative, old blockers, and superseded discoveries belong in `.context/PROJECT_HISTORY.md`, not in `.context/STATE.md`.
- Update `.context/PLAN.md` whenever the current plan changes, a plan item completes, or the next execution order becomes clearer.
- `.context/PLAN.md` should be an execution plan for the current task, not just a broad roadmap.
- `.context/PLAN.md` should normally use three primary sections: `Steps Completed`, `In Progress`, and `Steps Remaining`.
- `.context/STATE.md` should map directly to the `In Progress` section of `.context/PLAN.md` and act as the compact live memory for that active subtask.
- If `PLAN.md` and `STATE.md` drift apart, the agent MUST bring them back into alignment immediately.
- Append to `.context/PROJECT_HISTORY.md` after significant work.
- `.context/TASK.md` is the task definition, not the execution log.
- `.context/TASK.md` should stay short and stable during task execution.
- `.context/TASK.md` should contain only:
  - the active task statement
  - scope or constraints that define the task
  - definition of done
- `.context/TASK.md` MUST NOT accumulate completed-step history, debugging notes, or progress narrative. Those belong in `.context/PLAN.md` and `.context/PROJECT_HISTORY.md`.
- If there is no active task, `.context/TASK.md` should say that plainly and remain minimal.
- Execution progress, subtask status, and next implementation steps belong in `.context/PLAN.md`.
- Keep resource files updated via `.context/resources/README.md` index.
- When truck-model files are added or changed, the agent MUST update `.context/resources/truck_models/README.md` and keep the active truck model declaration in `AGENTS.md` accurate if the project focus has changed.
- When the user requests an illustration prompt, diagram prompt, architecture poster prompt, or similar visual-generation prompt, the agent MUST first consult `.context/ILLUSTRATION_FORMAT.md` and use it as the default style baseline unless the user explicitly asks for a different style.
- Illustration prompts should keep on-image text to the minimum necessary for understanding, because current image models are poor at rendering dense text cleanly.
- Use the root `CONFLICTS.md` as the formal conflict register and keep it in table format.
- Use the root `DECISIONS.md` to log significant project decisions, especially around conflicts, deviations, and architectural direction. Do not use it for context-maintenance housekeeping or backlog-only deferrals.
- Whenever any file is added to or deleted from the repository, the agent MUST document the change in `.context/PROJECT_HISTORY.md`, including why the file was added or removed.
- Prefer the absolute minimum design that satisfies the current project need while still fully meeting safety, security, privacy, correctness, and project-rule requirements. If the agent believes extra features, generalization, or flexibility beyond that minimum would be useful, it MUST present that as an explicit user decision instead of adding it unasked.
- Code should include enough concise, intent-level comments to be understandable by another programmer or another agent without guesswork. Comments should explain why, assumptions, task/concurrency constraints, or non-obvious control flow, not restate trivial code line by line.
- Whenever a shared variable is added, removed, renamed, repurposed, or has its unit or encoding changed, the agent MUST update `docs/VARIABLE_MODEL.md` in the same work item so the variable reference remains the maintained source-of-truth.
- Before mining a datasheet for a chip-related task, the agent SHOULD first check whether a corresponding Markdown note already exists under `.context/resources/chip_notes/` and whether it already contains the needed project-specific fact. Datasheets remain the authority, but chip notes should be the first stop for reused project knowledge.
- When an agent extracts a durable chip fact from a datasheet, bench test, or verified code review and that fact affects code, docs, configuration, or decisions, it MUST update the corresponding Markdown note under `.context/resources/chip_notes/` if one exists. If no chip note exists and the finding is important enough to reuse, the agent SHOULD create a new Markdown note there and index it from the local resource README.
- Chip notes must be written in Markdown and remain useful to both humans and agents: concise, project-focused, and explicit about what is verified fact versus inference.
- Chip notes must stay centered on the chip itself: registers, timing, conversions, electrical or protocol facts, quirks, and reusable conclusions. Project variable mappings, task-level data contracts, and other firmware-specific naming schemes belong in project docs such as `docs/VARIABLE_MODEL.md`, not in chip notes.
- Chip notes must NEVER present project-specific behavior, project configuration, or one firmware path's choices as chip fact. They should be written in terms of default behavior, alternate modes, common use, best practice, or clearly labeled inference from a specific code path when that context is truly needed.
- After meaningful progress (especially after debugging struggles), remind the user that making a git commit checkpoint is a good idea.