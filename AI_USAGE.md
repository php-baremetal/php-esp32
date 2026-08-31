# AI Usage Policy

This project uses AI as a tool, and would rather be upfront about it than leave anyone guessing. This
document is the canonical statement of **how AI is used here** and **what's expected of contributions
made with it**. The README and CONTRIBUTING carry short summaries that point back here.

The short version: an AI assistant is welcome as a tool, the same way an IDE or a compiler is. It does
not get to be the engineer. Every line that ships is understood, owned, and verified by a human.

## How AI is used in this project

- **The engineering is human.** The architecture, the design decisions, and the approach to each
  problem are made and debugged by hand — cross-compiling the *unmodified* Zend engine for the chip,
  the build-time patches for the bare-metal quirks (setjmp, the allocator, tz/csprng/session), the SAPI
  and the hardware bring-up. The proof is that it runs on real hardware.
- **Where AI helps.** Mostly the website and the documentation *prose*, where an LLM helps with the
  wording (the DMD doc format itself is the project's own). On the code it stays an assistant: reasoning
  through problems, mechanical and *verifiable* work (for example diffing the sources to confirm the
  port patches still apply across a version bump), and writing comments that describe the code accurately.
- **When a project is genuinely AI-heavy** — design and engineering included — that gets stated openly.
  This one isn't: the AI is a tool on top of human engineering.

## Policy for contributions

Using an AI assistant to help write a contribution is fine. What matters is not *whether* a tool was
used but that the result is correct and owned. Concretely:

- **You must understand and stand behind your code.** Don't open a PR of generated code you can't
  explain. If a reviewer asks "why does this work?", you should be able to answer.
- **Hardware verification is mandatory, AI or not.** As with any change, a PR that affects behaviour
  must be flashed to a real board and confirmed working, with the board(s), PHP version(s) and serial
  output stated. See [CONTRIBUTING.md](CONTRIBUTING.md). Generated code that merely compiles is not a
  contribution.
- **Hold to the project's principles.** The engine stays unmodified (changes are build-time patches),
  the manifest stays the source of truth (`check-manifest` green), and features stay opt-in. An AI
  suggestion that breaks these is wrong for this project even if it's valid PHP or valid C.
- **No unreviewed bulk output.** Don't submit large machine-generated changes you haven't read line by
  line. Small, reviewable commits are preferred.
- **Respect licences and provenance.** Don't paste in code an assistant reproduced from an
  incompatible-licence source. Contributions are under this project's [MIT License](LICENSE); the
  vendored PHP source stays under the [PHP License](https://www.php.net/license/).

## Disclosure

You don't need to annotate every line an assistant helped with. But if a contribution is *substantially*
AI-generated (a whole extension, a large doc, a non-trivial patch), a one-line note in the PR is
appreciated, so review can weigh it accordingly. Honesty here is valued, never penalised.

## Why this stance

Running the real PHP engine on bare metal is unforgiving: there's no OS to catch a mistake, and a
change that looks right but wasn't verified can brick a board or corrupt the heap silently. That's why
the bar is understanding plus hardware proof, not the absence of tooling. AI that raises the quality
and speed of well-understood, verified work is welcome. AI used to skip the understanding or the
verification is not.
