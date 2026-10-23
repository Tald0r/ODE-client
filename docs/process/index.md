# Rules

* [Commit messages](commit-messages.md) - A commit is type: lowercase imperative summary, then prose on why the change is right, what was verified and what is out of scope; a fix: commit names its Test path, and the commit-msg hook refuses one without.
* [Measure the value, not the spelling](measure-the-value.md) - A checker walks the value a site uses, not one spelling of it; a search that finds nothing is a fact about the search; and a reference on a live line can still be dead, so find the caller that runs before describing a seam.
* [Move, then fix](move-then-fix.md) - Moving a file between targets does not change its bytes in the same commit when it has no seam to cut: move first, prove the build, then fix, so the diff that changes behaviour is readable.
* [Restructuring status lines](restructuring-status-lines.md) - Every task in the restructuring plan has a checkbox and a Status line updated in the same commit as the work; a task is done only once its Owner exists, and a substantial task gets an adversarial review by two reviewers before merge.
* [Scripted rewrites](scripted-rewrites.md) - A scripted rewrite bounds its pattern, because a non-greedy match once swallowed 87 lines into one argument that no ratchet could see, and is followed by a sweep for orphaned banners and //#include lines.
* [Source style](source-style.md) - Tabs, English only, Hungarian notation and banner comments as the surrounding file has them; line endings vary by file and each keeps its own, so edit with the Edit tool or perl -pi on the exact files, never sed -i or awk.
