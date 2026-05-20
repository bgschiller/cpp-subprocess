import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import * as fs from "node:fs";
import * as path from "node:path";

export default function (pi: ExtensionAPI) {
  pi.registerCommand("next-ticket", {
    description: "Read the next ticket and work on it (TDD, docs, follow-ups, then commit)",
    handler: async (_args, ctx) => {
      const ticketsDir = path.join(ctx.cwd, "tickets");

      // Find the lowest-numbered ticket file
      let files: string[];
      try {
        files = fs.readdirSync(ticketsDir);
      } catch {
        ctx.ui.notify("No tickets/ directory found.", "error");
        return;
      }

      const ticketFiles = files
        .filter((f) => /^\d+.*\.md$/.test(f))
        .sort((a, b) => {
          const numA = parseInt(a.match(/^(\d+)/)?.[1] ?? "0", 10);
          const numB = parseInt(b.match(/^(\d+)/)?.[1] ?? "0", 10);
          return numA - numB;
        });

      if (ticketFiles.length === 0) {
        ctx.ui.notify("No tickets found in tickets/ — all done!", "info");
        return;
      }

      const nextTicket = ticketFiles[0];
      const ticketPath = path.join(ticketsDir, nextTicket);
      const ticketContent = fs.readFileSync(ticketPath, "utf8");

      ctx.ui.notify(`Working on: ${nextTicket}`, "info");

      await ctx.waitForIdle();

      pi.sendUserMessage(
        `Please work on the following ticket: \`tickets/${nextTicket}\`

---

${ticketContent}

---

## Instructions

Work through this ticket carefully, following these guidelines:

1. **TDD where possible** — write or update tests in \`test/src/\` *before*
   or alongside the implementation. All new behaviour must have corresponding
   tests. Use GoogleTest. Qualify types with the \`subprocess::\` namespace
   (no \`using namespace\` declarations).

2. **Push back on the design if it doesn't make sense** — if any part of the
   ticket's proposed approach conflicts with the existing codebase, introduces
   unsoundness, or has a clearly better alternative, say so and propose the
   improvement before proceeding.

3. **Update documentation where appropriate** — update Doxygen comments in
   affected headers, the README if the public API changes, and any other
   in-tree docs that reference the changed code.

4. **Create new tickets for follow-up tasks** — if you discover additional
   work that is out of scope for this ticket but shouldn't be forgotten,
   create a new numbered \`.md\` file in \`tickets/\` following the existing
   naming convention (e.g. \`14-short-description.md\`).

5. **Delete the ticket when done** — once the work is complete and all tests
   pass, delete \`tickets/${nextTicket}\`.

6. **Commit** — stage all changed files and create a single commit with a
   clear, conventional-style message summarising the work (e.g.
   \`feat: implement Result<T> map/and_then and fix or_throw\`). Run
   \`cmake --build build --target clang-format\` first so the commit is
   clean; confirm tests pass with \`cd build && ctest -C Release -VV\`.
`,
        { deliverAs: "followUp" },
      );
    },
  });
}
