# Agent instances

An instance is one delegated execution with its own ID and retained context.
Agent definitions under `/agents` configure how work starts; `/subagents` shows
what actually ran in the current session. `/tasks` keeps its scheduler meaning.
The Tasks checklist above the prompt describes the parent's plan, while Subagents
and Agent Teams remain separate activity groups with indented members.

## Open an instance

Use `/subagents` or click the Subagents heading. The browser starts with Active
instances. Press Tab for History, Up/Down to select, and Enter for the instance conversation.
Press **i** on a selected row for metadata and failure Details.
Left/Right switches between Subagents and Team Agents. `/subagents history`
opens ordinary history; `/subagents team` opens the native Team Agent list.
`/subagents <full-agent-id>` opens a particular current-session conversation.
Escape returns from Details to the list, then closes the browser.

The browser is available while the parent is working. A permission request takes
precedence and closes it so that the original permission decision stays usable.
Reopen the browser afterward. Ctrl-D exits and Ctrl-L redraws. Ctrl-C interrupts
the currently open conversation, which can differ from the highlighted list row.
To stop another instance, Enter its conversation first or use Stop in its Details.
Other browser input stays inside the modal.

## Work inside a child conversation

The header identifies Main, the category and status, then the selected instance.
Long names may be clipped on narrow terminals; Details retain the full ID.
Child sessions show individual message and tool cards with their actual status.
Type directly into the normal editor to send plain text to that instance.
Running workers queue input for their next available model turn; the transcript
labels queued input until it is consumed. Accepted input can remain pending if
the worker exhausts its budget; a notice requires an explicit continuation.
Older pending input precedes newer continuation text. Ended local workers resume
with the same ID and retained context under their existing definition and policy.
Native Team continuation reuses the existing member; removed membership prevents it.

Use **Alt+Left** or `/back` to return to Main. Root and child drafts, scroll and
history remain separate. Escape opens the instance list; `/subagents` does too.
Ctrl-C or `/stop` requests cancellation only for the selected child. Ctrl-D and
`/exit` exit the application. Switching views never stops a worker or consumes
its parent's completion notification. Main keeps working in the background.

Text and bracketed paste are supported. App-level transcript drag-copy and
hyperlink launching remain Main-only in this phase. Other slash commands, local shell mode
and attachments are explicitly unavailable in this view and preserve the draft;
use `/back` to operate Main. Usage statistics for child workers are unavailable,
so the view does not present Main's token/cost numbers as child measurements.

Permission and MCP questions temporarily take precedence over the child view;
answer the original request, then return to the retained child draft. Stop can
remain pending while an unanswered external question is reachable; deny that
request to let cleanup proceed. Stopping one child never declines another
worker's request. Native Agent AskUser remains prohibited by existing policy.

## Inspect, continue and stop

Details show the instance category, status, ID, type, description, current failure
reason, output and recent transcript. Up/Down, Page Up/Page Down and the mouse
wheel scroll. Text sections are bounded to 16 KiB; the transcript is limited to
80 lines, with a truncation notice. Terminal control sequences are removed.
Details do not follow paths stored in records or consume completion notifications.

Press **c** to edit a continuation instruction, then Enter to launch. Continue
keeps the same ID and context and applies the current definition, budget and tool
permissions. It is a new Agent turn, not a process checkpoint. It can still fail.
The browser closes before launch so that permission requests remain reachable.
Launch accepted means execution started; the usual activity and completion events
report its result. The host does not fabricate model tool history for this action.

Continue is available for ended direct local ordinary subagents with retained
context and a resolvable definition. The parent must be idle, and earlier cleanup
must finish. A reason explains unavailable controls. Native Team workers can also continue when the original team/member still exists.
Nested and external pane workers cannot be continued here. Concurrent resumes of one ID
are rejected before startup hooks or record mutation.

Press **s** to request Stop for an actual execution owned by this process and
session. Stop remains available while the parent is busy. A stopping indicator
means cancellation was requested; the worker publishes the final state after its
existing cleanup. Stale records and another process's workers are not marked
stopped. External pane-only teammates remain in `/teams` with their original
Running/Idle/Unknown semantics and backend controls.

## Failure and history

Ended ordinary instances, including failures and cancellations, fold into the
Subagents heading. Failed counts report facts; they do not imply that you owe a
response. Open History to inspect the reason or continue eligible work. Folding
never deletes records, acknowledges model notifications or changes a failed
result into success. `/clear` keeps the current session's instance history.
An instance can fail after a tool succeeded, for example when its turn budget
runs out. Earlier successful tools keep their success state; cancelled or failed
calls show their own error state. Older retained histories may lack that recorded
tool outcome and are not automatically rewritten.
A continuation can replace current output/error; this view does not claim to
provide an immutable per-attempt audit trail. Native transcripts and session
traces remain the evidence for earlier work.
