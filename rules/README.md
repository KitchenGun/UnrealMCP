# Rules Directory

Add rules here each time a failure occurs.

## Rule File Format

### .md -- Human-readable rule description
Record why this rule was created and what failure led to it.

### .sh -- Automated checks run by linter
`./scripts/lint.sh` runs all .sh files in this directory.
If first argument is "true", it's in auto-correction mode.

```bash
#!/bin/bash
# Rule script template
FIX=${1:-false}
FOUND=0
# Check logic...
[ $FOUND -gt 0 ] && exit 1
exit 0
```
