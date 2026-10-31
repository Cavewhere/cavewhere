#!/bin/sh
# Install the private-data pre-commit and commit-msg hooks into this clone's git hooks
# directory (shared by every worktree). Each hook runs the checked-out privatecheck.py.
set -e
hooks_dir="$(git rev-parse --git-common-dir)/hooks"
mkdir -p "$hooks_dir"

write_hook()
{
    target="$hooks_dir/$1"
    if [ -e "$target" ] && ! grep -q privatecheck "$target"; then
        echo "install-hooks: $target exists and is someone else's hook; add this line to it:" >&2
        echo "  $2" >&2
        return
    fi
    cat > "$target" <<HOOK
#!/bin/sh
# Installed by .claude/skills/private-dataset/scripts/install-hooks.sh
check="\$(git rev-parse --show-toplevel)/.claude/skills/private-dataset/scripts/privatecheck.py"
[ -f "\$check" ] || exit 0
$2
HOOK
    chmod +x "$target"
    echo "install-hooks: wrote $target"
}

write_hook pre-commit 'exec python3 "$check" --staged'
write_hook commit-msg 'exec python3 "$check" --message "$1"'
