<!-- SPDX-License-Identifier: CC-BY-4.0 -->
<!-- Copyright Contributors to the OpenImageIO Project. -->

# Agentic coding / coding assistants

Please start by reading [our policy on using "AI" coding
assistsants](AI_Policy.md).


## Multi-tool setup

We aim to allow people to use whatever agentic or coding assistant tools they
want. The repository ships with a setup script that configures tool-specific
files locally for whichever assistant(s) you use.

### Generic: `AGENTS.md` and `.agents/`

As much as possible, we use identical instructions and files across all coding
assistants.

`AGENTS.md` is the main file providing overall repo-specific instructions and
context to agents. Many coding agents already read this file directly; for
those that don't, the setup script creates the necessary links or wrappers.

`.agents/skills/` is where shared skill files live. Many tools find skills
here automatically; for those that need them elsewhere, the setup script
creates the appropriate links.

### Running the setup script

After cloning, run `.agents/setup-agent <tool>` for the tool(s) you use:

```
.agents/setup-agent claude          # Claude Code
.agents/setup-agent cursor          # Cursor
.agents/setup-agent codex           # OpenAI Codex
.agents/setup-agent opencode        # Opencode
.agents/setup-agent copilot         # GitHub Copilot
.agents/setup-agent all             # all of the above
```

The script is idempotent — running it multiple times is safe. To undo:

```
.agents/setup-agent clear           # remove all tool setup
.agents/setup-agent clear claude    # remove setup for one tool
```

Tool-specific directories (`.claude/`, `.cursor/`, `.codex/`, `.opencode/`,
`.github/copilot-instructions.md`) are created locally only and are listed in
`.gitignore`. Each developer runs the script for the tool they use; nothing
tool-specific is committed to the repository.

The script header contains per-tool documentation (what each tool reads
natively, what requires setup, and links to relevant docs) — consult it when
adding support for a new tool or updating the strategy for an existing one.

