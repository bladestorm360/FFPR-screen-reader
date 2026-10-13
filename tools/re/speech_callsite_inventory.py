"""Index central speech call sites in core/all game adapters; no runtime access."""
from pathlib import Path
from collections import Counter
import re

ROOT = Path(__file__).resolve().parents[2]
rows = []
for path in sorted((ROOT / 'src').rglob('*.cpp')):
    source = path.read_text(encoding='utf-8-sig')
    for match in re.finditer(r'\b((?:ffpr|speech)::say(?:_sequence|_parts|_queued)?)\s*\(', source):
        prefix = source[source.rfind('\n', 0, match.start()) + 1:match.start()]
        if prefix.lstrip().startswith('//'):
            continue
        depth, braces, quote, escaped = 1, 0, '', False
        args, start, i = [], match.end(), match.end()
        while i < len(source) and depth:
            c = source[i]
            if quote:
                if escaped:
                    escaped = False
                elif c == '\\':
                    escaped = True
                elif c == quote:
                    quote = ''
            elif c in '\"\'':
                quote = c
            elif c == '{':
                braces += 1
            elif c == '}':
                braces -= 1
            elif c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if not depth:
                    args.append(source[start:i].strip())
            elif c == ',' and depth == 1 and braces == 0:
                args.append(source[start:i].strip())
                start = i + 1
            i += 1
        api = match[1]
        policy = 'queued' if api.endswith('_queued') else 'default interrupt'
        if len(args) > 1:
            flag = args[1]
            policy = {'false': 'queued', 'true': 'interrupt'}.get(flag, 'context expression')
        line = source.count('\n', 0, match.start()) + 1
        rows.append((path.relative_to(ROOT).as_posix(), line, api, policy))
target = ROOT / 'docs/re/speech_calls.tsv'
target.write_text('source\tline\tAPI\tpolicy\n' + ''.join('\t'.join(map(str, row))+'\n' for row in rows), encoding='utf-8')
counts = Counter(row[3] for row in rows)
print(f'{len(rows)} speech calls in {len({row[0] for row in rows})} source files: ' + ', '.join(f'{key}={value}' for key,value in sorted(counts.items())))
