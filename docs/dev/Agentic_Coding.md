<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# Agentic coding / coding assistants

Please start by reading [our policy on using "AI" coding
assistsants](AI_Policy.md).


## Multi-tool setup

We aim to allow people to use whatever agentic or coding assistant tools they
want, and in some cases we provide context, commands, etc., specific to this
project, as part of the project. We have tried to set up the repo foe easy use
of several of the major tools.

### Generic: `AGENTS.md` and `.agents/`

A much as possible, we would like to use identical instructions and files
across all the coding assistants.

`AGENTS.md` is the main file providing overall repo-specific instructions and
context to agents. Many coding agents know to read that, but for the ones that
don't, we link their specially named file to this canonical file.

`.agents/` is the subdirectory where we prefer to keep all the resources that
can be reasonably shared among the different agentic tools. Individual tools
should link or refer to files in this area when possible.

`.agents/skills/` is where common "skill" files should be kept. Many coding
assistants already know to look for skills here, but for those that do not, we
link their custom skills directory to this area.

### Claude Code

Claude Code keeps its files in `.claude/`. Within that directory are:

- `.claude/CLAUDE.md` is a thin wrapper that refers to `AGENTS.md`. Please put
  any general instructions in AGENTS.md, and only put additional items in
  CLAUDE.md if they are Claude-specific. (We hope that eventually Claude will
  use AGENTS.md automatically, as many other tools increasingly are doing.)
- `.claude/skills` is a symbolic link to the shared `.agents/skills`.
- `.claude/.gitignore` is a list of things within `.claude/` which should not
  ever be committed to the repository (user session state, etc.).

We do not have a root-level `CLAUDE.md`, in order to avoid clutter. Claude
should naturally check `.claude/CLAUDE.md`, which in turn incorporates by
reference the main `AGENTS.md`.

### Codex

OpenAI Codex uses the main `AGENTS.md` file directly
([reference](https://developers.openai.com/codex/guides/agents-md)) and looks
for skills in `.agents/skills`
([reference](https://developers.openai.com/codex/skills)).

### Cursor

Cursor keeps its files in `.cursor/`:

- `.cursor/rules/project.mdc` is a symbolic link to the shared `AGENTS.md`.
- `.cursor/.gitignore` is a list of things within `.cursor/` which should not
  ever be committed to the repository (user session state, etc.).

Cursor doesn't seem to use quite the same skill format as the other tools.

### GitHub Copilot

GitHub Copilot documents that it already knows to use the main `AGENTS.md`
file ([reference](https://code.visualstudio.com/docs/copilot/customization/custom-instructions)) and `.agents/skills/` ([reference](https://docs.github.com/en/copilot/concepts/agents/about-agent-skills)). No links or custom files should be needed.

### Opencode

Opencode uses the main `AGENTS.md` file directly.

Opencode docs say it knows to find skills in `.agents/skills/`, so it should
be able to share the skills without any links. But in practice, we are having
trouble indepedently verifying if it works.

Opencode keeps other files in `.opencode/`:

- `.opencode/skills` is a symbolic link to the shared `.agents/skills`.
- `.opencode/commands/` contains individual symbolic links to the shared
  skills in `.agents/skills`. This seems to necessary despite the
  contradiction with the Opencode documentation.
- `.opencode/.gitignore` is a list of things within `.opencode/` which should
  not ever be committed to the repository (user session state, etc.).

