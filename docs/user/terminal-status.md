# Terminal program status

Loom can report its interactive status to terminals that support OSC 7501
(an operating system command escape sequence for program status). The terminal
decides how to show these reports, for example in its tab or pane indicators.

No configuration is required. Loom sends one capability query after entering
the interactive terminal loop. Reporting starts only if a compatible reply
arrives within one second. Both input and output must be terminals. A terminal
that does not reply continues to work normally, without status reports.

| Status | Meaning |
|---|---|
| `idle` | Ready for input, or the foreground operation was cancelled. |
| `working` | A foreground model operation or local `!` shell command is running. |
| `blocked`, `kind=permission` | An actual authorization request needs a decision. |
| `blocked`, `kind=question` | A question or tool-round continuation needs an answer. |
| `done` | The whole foreground operation finished successfully. |
| `error` | The foreground operation stopped with an error, including a failing local shell command. |

Authorization takes priority over a question, and both take priority over
working. Detached background jobs do not create separate terminal records;
their requests can still require a decision. A terminal that supports blocked
status but does not advertise a particular kind receives `blocked` without
that kind.

In the current ask-user dialog, Enter submits a supplied default answer and
Escape cancels the question.

Completion and failure remain until you type, click, scroll, or start another
operation. Redrawing, resizing, focus changes and mouse movement preserve
them. Answering a pending decision also preserves a prior foreground result.
Exiting directly with Ctrl-D preserves an unacknowledged completion or failure;
exiting while work is active reports idle. Interrupted or abnormal shutdown
attempts an idle report before restoring the terminal. A lost terminal
connection or forced kill can prevent that final report.

Status reports contain only fixed state/kind values and `app=loom`. They do
not contain prompts, tool arguments, paths or results. This integration does
not change permissions, shell exit codes, session files or model requests.

Support depends on the terminal application, not just its parser library.
See the [program-status specification](https://www.superlogical.com/rex/docs/build/program-status)
and [RFC 0016](../rfcs/0016-terminal-program-status.md) for protocol details.
