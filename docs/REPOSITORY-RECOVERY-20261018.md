# GitHub recovery (2026-10-18)

The user reported accidentally removing the repository's history and requested
the current project be restored before development continued.

Read-only checks found:

- Local master retained 175 commits through `029e81a`.
- Remote master was the single unrelated commit `17ae7a7`, titled
  `Reset repository history`, with an empty tree.
- In-progress metadata changes were still present locally and left untouched.

Recovery joined the empty reset commit to the complete saved project history:

```
git merge --allow-unrelated-histories -s ours origin/master \
  -m "Restore full project history after accidental GitHub reset"
git push origin master
```

No force-push, reset, deletion, stash or data cleanup was used. Recovery commit
`202263be12f275c2baf4ae27a47982ac94c0712d` was confirmed as remote master. Its 419
tracked files exactly match `029e81a`; that prior tip remains its ancestor. The
joined graph has 177 reachable commits, including the reset and recovery commits.

This restores the tracked port source, tests, documentation and history.
Ignored original commercial game files, local saves, dependencies and generated
build/capture artifacts remain local according to the existing `.gitignore`;
they were not indiscriminately uploaded. Subsequent verified development commits
build on this recovered history.
