# openvela AI Skills

**English** | [中文](README_zh-cn.md)

AI-powered development skills for [openvela](https://github.com/open-vela) (NuttX-based RTOS). Each skill teaches the AI assistant domain-specific knowledge about embedded system development, debugging, and optimization.

## Skills

| Skill                                                        | Description                                                                                                                    |
| ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------ |
| [codesize](skills/codesize/)                                 | Analyze firmware binary size across multi-core/multi-architecture (ARM/Xtensa/RISC-V) targets                                  |
| [executor](skills/executor/)                                 | Manage persistent interactive CLI processes (REPLs, debuggers, QEMU, NuttX simulator)                                          |
| [kconfig-tweak](skills/kconfig-tweak/)                       | Modify NuttX/Linux .config files from command line without interactive menuconfig                                              |
| [memdump](skills/memdump/)                                   | Analyze heap memory usage from NuttX runtime memdump logs, detect leaks and high-consumption modules                           |
| [pcm-audio](skills/pcm-audio/)                               | Analyze PCM audio quality issues — clipping, silence, clicks, noise floor, periodic distortion                                 |
| [skill-creator](skills/skill-creator/)                       | Guide for creating new skills that extend AI assistant capabilities                                                            |
| [tmux](skills/tmux/)                                         | Remote control tmux sessions for interactive CLIs (python, gdb, etc.)                                                          |
| [openvela-quickstart](skills/openvela-quickstart/)           | Set up openvela dev environment from scratch — detect env, install deps, smart source selection, build & run emulator          |
| [openvela-build](skills/openvela-build/)                     | Build openvela firmware, edit kernel config (menuconfig), run the emulator, and fix build errors                               |
| [nuttx-driver-development](skills/nuttx-driver-development/) | Create/update/review NuttX device drivers — sensor, char, network, fb/LCD, USB, audio, power/battery, MCAL, I2C/SPI subsystems |
| [driver-code-reviewer](skills/driver-code-reviewer/)         | NuttX/Vela driver code-quality review (59 patterns + two-pass cross-validation + quantitative scoring)                         |
| [submit-pr](skills/submit-pr/)                               | Submit Pull Requests to the openvela community (GitHub/Gitee) via fork mode, with single/multi-repo batch support              |
| [contest-log-collector](skills/contest-log-collector/)       | openvela AI Contest log auto-collector (OpenCode/Claude Code/Codex/AIoT-IDE — zero-config session capture)                     |

## Agents

| Agent                                              | Description                                                                                                                                                                    |
| -------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [driver-workflow](agents/driver-workflow.agent.md) | End-to-end NuttX driver development workflow agent — new driver, improve existing, code review, test generation (6-step flow / 3 checkpoints, from requirements to submission) |

## Quick Start

Clone this repository into your openvela project root as `.claude/`:

```bash
git clone https://github.com/open-vela/ai-skills.git .claude
```

The AI assistant will automatically discover and use these skills when relevant tasks are requested.

## Skill Structure

Each skill follows a standard layout:

```
skills/<skill-name>/
├── SKILL.md              # Skill definition (required)
├── scripts/              # Helper scripts (optional)
├── references/           # Reference documentation (optional)
└── LICENSE               # License file (optional, defaults to repo license)
```

`SKILL.md` uses YAML front matter for metadata:

```yaml
---
name: skill-name
description: When and how to use this skill
---
```

## Creating New Skills

Use the `skill-creator` skill to generate new skills interactively, or follow the structure above manually.

## Contributing

Contributions are welcome. Please open an issue or pull request.

When adding a new skill:
1. Create a directory under `skills/` with a descriptive name
2. Write a `SKILL.md` with clear trigger conditions and step-by-step instructions
3. Include helper scripts in `scripts/` if needed
4. Add reference docs in `references/` for domain knowledge

## License

Apache-2.0
