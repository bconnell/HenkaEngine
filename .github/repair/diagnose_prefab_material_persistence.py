from pathlib import Path

path = Path("tests/test_sandbox3d_game_authoring.c")
text = path.read_text(encoding="utf-8")
token = "static bool test_prefab_material_override_persists_through_authoring(void)"
start = text.find(token)
if start < 0:
    raise RuntimeError("target test function not found")
end = text.find("\nstatic ", start + len(token))
if end < 0:
    raise RuntimeError("target test function end not found")
end += 1
fn = text[start:end]
needle = "goto cleanup;"
indices = []
pos = 0
while True:
    idx = fn.find(needle, pos)
    if idx < 0:
        break
    indices.append(idx)
    pos = idx + len(needle)
if len(indices) < 6:
    raise RuntimeError(f"unexpected cleanup count: {len(indices)}")
print("CHECKPOINT MAP")
for i, idx in enumerate(indices, 1):
    context = " ".join(fn[max(0, idx - 280):idx].split())
    print(f"checkpoint {i}: ...{context}")
inst = fn
for i in range(len(indices) - 1, -1, -1):
    idx = indices[i]
    line_start = inst.rfind("\n", 0, idx) + 1
    indent_len = 0
    while line_start + indent_len < len(inst) and inst[line_start + indent_len] == " ":
        indent_len += 1
    indent = inst[line_start:line_start + indent_len]
    replacement = (
        f'fprintf(stderr, "HENKA_PREFAB_MATERIAL_DIAG_FAIL checkpoint={i + 1}\\n");\n'
        f"{indent}goto cleanup;"
    )
    inst = inst[:idx] + replacement + inst[idx + len(needle):]
path.write_text(text[:start] + inst + text[end:], encoding="utf-8", newline="\n")
