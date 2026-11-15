#!/usr/bin/env python3
"""Inventory explicit source dispatches; does not execute native gameplay."""
import ast,hashlib,json,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
def condition(node,number):
 if isinstance(node,ast.Expression):return condition(node.body,number)
 if isinstance(node,ast.Name) and node.id=='id':return number
 if isinstance(node,ast.Constant) and isinstance(node.value,int):return node.value
 if isinstance(node,ast.BoolOp):
  values=[condition(x,number) for x in node.values]
  return all(values) if isinstance(node.op,ast.And) else any(values)
 if isinstance(node,ast.Compare):
  values=[condition(node.left,number),*[condition(x,number) for x in node.comparators]]
  compare={ast.Eq:lambda a,b:a==b,ast.NotEq:lambda a,b:a!=b,ast.Lt:lambda a,b:a<b,ast.LtE:lambda a,b:a<=b,ast.Gt:lambda a,b:a>b,ast.GtE:lambda a,b:a>=b}
  return all(compare[type(op)](values[i],values[i+1]) for i,op in enumerate(node.ops))
 raise ValueError('unreviewed dispatch condition')
def main():
 source=(R/'src/play_world.c').read_text();start='static int spawn_template(struct NativePlayWorld*w,uint16_t id,const struct PlayerTransform*t,const uint8_t params[8],uint8_t flags,enum EntityInsert order){';body=source[source.index(start):];body=body[:body.index('\nint play_world_spawn_template')]
 # Reviewed item fallback:284..315;292 uses the explicit fixed Slug request;
 # 293 uses the explicit crate branch;316 is the combo controller.
 items=(R/'src/items.c').read_text()
 if 'if(id<284||id>315||id==292)return 0;' not in items or 'if(id==316)return item_spawn_combo_controller' not in items:raise ValueError('item fallback changed; review required')
 entries=set(range(284,317));predicates=[]
 for line in body.splitlines():
  if not line.startswith(' if('):continue
  depth=1;i=4
  while depth and i<len(line):
   if line[i]=='(':depth+=1
   elif line[i]==')':depth-=1
   i+=1
  if depth:raise ValueError('multiline predicate requires review')
  expr=line[4:i-1]
  if re.sub(r'\bid\b|[\d\s()&|=<>!]','',expr):continue
  predicates.append(expr);tree=ast.parse(expr.replace('&&',' and ').replace('||',' or '),mode='eval')
  entries.update(n for n in range(329) if condition(tree,n))
 entries.update(int(v) for v in re.findall(r'case (\d+):',body))
 authored=json.loads((R/'generated/mission-source.json').read_text())['templates'];missing=[t for t in authored if t['id'] not in entries]
 result=dict(version=(R/'VERSION').read_text().strip(),method='Static constructor dispatch inventory only, not execution or completed behavior coverage',released_game_connected=False,complete=False,source_sha256=hashlib.sha256(source.encode()).hexdigest(),native_dispatch_ids=sorted(entries),unwired_ids=list(sorted(set(range(329))-entries)),authored_template_count=len(authored),authored_dispatch_count=len(authored)-len(missing),authored_missing=missing,known_pending_paths=['59/063106 incoming selector unresolved; typed explicit selector required','120/040554 ambient D5 radius unresolved','318/045F1A inherited renderer geometry unresolved','All constructor dispatches do not establish complete behavior, scene or rendering integration'],predicates=predicates)
 (R/'generated/native-template-inventory.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(dispatches=len(entries),authored=len(authored),authored_missing=len(missing),unwired=len(result['unwired_ids']))))
if __name__=='__main__':main()
