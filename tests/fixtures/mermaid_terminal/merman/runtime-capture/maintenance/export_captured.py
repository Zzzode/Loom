from pathlib import Path
import collections,hashlib,json,re,shutil,sys
ROOT=Path(sys.argv[1]).resolve();S=ROOT/'build/mermaid-renderer-research-20261019';UP=S/'merman-observer-execution';PKG=UP/'crates/merman-ascii';OLD=ROOT/'tests/fixtures/mermaid_terminal/merman';DEST=S/'merman-runtime-import';A=DEST/'runtime-capture';A.mkdir(parents=True,exist_ok=True)
def sha(b):return hashlib.sha256(b).hexdigest()
def dump(p,x):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(x,ensure_ascii=False,sort_keys=True,indent=2)+'\n')
def canonical(x):return (json.dumps(x,ensure_ascii=False,sort_keys=True,separators=(',',':'))+'\n').encode()
rows=[json.loads(l) for l in (S/'merman-executed-inputs-exact.jsonl').read_text().splitlines()]
TOKEN=re.compile(r'r(?P<h>#{0,16})".*?"(?P=h)|b?"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\\n])\'|//[^\n]*|/\*.*?\*/',re.S)
def mask(t):return TOKEN.sub(lambda m:''.join('\n' if c=='\n' else ' ' for c in m.group()),t)
def functions(p):
 t=((S/'merman-original-ascii-lib.rs').read_text() if p==PKG/'src/lib.rs' else p.read_text());m=mask(t);out=[]
 for f in re.finditer(r'\bfn\s+(\w+)\s*\(',m):
  i=m.find('{',f.end());semi=m.find(';',f.end())
  if i<0 or (semi>=0 and semi<i):continue
  depth=1;j=i+1
  while j<len(m) and depth:depth+=(m[j]=='{')-(m[j]=='}');j+=1
  if depth:continue
  out.append({'name':f.group(1),'start':f.start(),'end':j,'line_start':t.count('\n',0,f.start())+1,'line_end':t.count('\n',0,j)+1})
 return t,out
sources={};func_index=collections.defaultdict(list)
for p in sorted([*(PKG/'tests').rglob('*.rs'),*(PKG/'src').rglob('*.rs')]):
 t,fs=functions(p);rel=str(p.relative_to(PKG));sources[rel]=(p,t,fs)
 for f in fs:func_index[f['name']].append((rel,f))
def orig_path(p):
 p=p.replace(str(UP)+'/', '')
 return p.removeprefix('crates/merman-ascii/')
assertions={}
def assertion_for(test,caller):
 fn=test.split('::')[-1];opts=func_index.get(fn,[])
 if not opts:return None
 caller=orig_path(caller);match=next(((p,f) for p,f in opts if p==caller),opts[0]);rel,f=match;p,t,fs=sources[rel];key=rel+':'+str(f['start'])+':'+str(f['end'])
 if key not in assertions:
  raw=t[f['start']:f['end']].encode();h=sha(raw);path=Path('runtime-capture/assertions')/(h+'.rs');target=DEST/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
  assertions[key]={'path':str(path),'sha256':h,'upstream_path':'crates/merman-ascii/'+rel,'original_file_sha256':sha(t.encode()),'byte_start':len(t[:f['start']].encode()),'byte_end':len(t[:f['end']].encode()),'line_start':f['line_start'],'line_end':f['line_end'],'test_name':test,'kind':'exact_original_test_function_slice'}
 return assertions[key]
familymap={'flowchart-v2':'flowchart','flowchart-elk':'flowchart','classDiagram':'class','classDiagram-v2':'class','stateDiagram':'state','stateDiagram-v2':'state','sequenceDiagram':'sequence','erDiagram':'er','gitGraph':'gitgraph','gitGraphDiagram':'gitgraph','xychart-beta':'xychart','treeView-beta':'treeview','treeView':'treeview','quadrantChart':'quadrant','swimlane-beta':'swimlane','requirementDiagram':'requirement','ishikawa-beta':'ishikawa'}
def family(row):
 md=row.get('metadata_debug') or '';m=re.search(r'diagram_type: "([^\"]+)"',md)
 if m:h=m.group(1)
 else:
  h=next((s.strip().split()[0].split(';')[0] for s in row['input'].splitlines() if s.strip() and not s.strip().startswith(('%%','---'))),'unknown')
 return familymap.get(h,h)
options={};metadata={};models={};outputs={};reports={};events=[];cases={};by_model=collections.defaultdict(list);parse_events=[];ascii_events=[];ambiguous=[];detached=[]
for i,row in enumerate(rows):
 row['_index']=i;isparse=row['event']=='parse_return';mdebug=row.get('model_debug');msha=sha(mdebug.encode()) if mdebug else None;md=row.get('metadata_debug');mdsha=sha(md.encode()) if md else None
 if mdsha:metadata[mdsha]=md
 if isparse:
  text=row['input'];h=sha(text.encode());c=cases.setdefault(h,{'id':'merman-runtime/'+h[:20],'family':family(row),'input':text,'sha256':h,'provenance':[],'expectation':{'classification':'parse_only','snapshots':[],'assertions':[],'obligations':[],'oracle_status':'runtime_public_source_captured; native_semantic_verifier_pending'}})
  model=row.get('upstream_semantic_model');modelsha=None
  if model is not None:
   raw=canonical(model);modelsha=sha(raw);models[modelsha]=raw;ref={'path':'runtime-capture/oracle-models/'+modelsha+'.json','sha256':modelsha,'bytes':len(raw),'source':'pinned public parse compatibility_json, maintenance only'}
   e=c['expectation'];e.setdefault('upstream_semantic_model',ref)
   if ref not in e.setdefault('semantic_model_variants',[]):e['semantic_model_variants'].append(ref)
  prov={'kind':'executed_public_parse','path':'crates/merman-ascii/'+orig_path(row['path']),'line_start':row['line'],'line_end':row['line'],'line_kind':'instrumented_callsite_only' if orig_path(row['path'])=='src/lib.rs' else 'original_unmodified_source','test_name':row['test'],'parse_options':row['parse_options'],'status':row['status'],'model_debug_sha256':msha,'metadata_sha256':mdsha,'model_json_sha256':modelsha,'capture_event':i}
  if row.get('error'):prov['error']=row['error']
  if prov not in c['provenance']:c['provenance'].append(prov)
  ar=assertion_for(row['test'],row['path'])
  if ar and ar not in c['expectation']['assertions']:c['expectation']['assertions'].append(ar)
  event={'index':i,'event':'public_parse','input_sha256':h,'model_debug_sha256':msha,'metadata_sha256':mdsha,'pid':row['pid'],'test_name':row['test'],'status':row['status'],'model_json_sha256':modelsha,'parse_options':row['parse_options']}
  if row.get('error'):event['error']=row['error']
  events.append(event);parse_events.append((row,event));by_model[(row['pid'],row['test'],msha)].append((row,event))
 else:
  optsha=sha(row['options_debug'].encode());options[optsha]=row['options_debug'];out=row.get('output');outsha=sha(out.encode()) if out is not None else None
  if outsha:outputs[outsha]=out.encode()
  report=row.get('report_debug');rsha=sha(report.encode()) if report else None
  if rsha:reports[rsha]=report
  ar=assertion_for(row['test'],row['path'])
  event={'assertion_source':ar['path'] if ar else None,'path':'crates/merman-ascii/'+orig_path(row['path']),'line':row['line'],'index':i,'event':'ascii_render','entry':row['entry'],'pid':row['pid'],'test_name':row['test'],'model_debug_sha256':msha,'metadata_sha256':mdsha,'options_sha256':optsha,'output_sha256':outsha,'report_sha256':rsha,'status':row['status'],'error':row.get('error')}
  matches=[(p,e) for p,e in by_model.get((row['pid'],row['test'],msha),[]) if p.get('model_debug')==row.get('model_debug')]
  if mdsha:matches=[(p,e) for p,e in matches if e['metadata_sha256']==mdsha and p.get('metadata_debug')==md]
  source_hashes={e['input_sha256'] for p,e in matches}
  event['candidate_source_sha256']=sorted(source_hashes)
  if len(source_hashes)==1:
   h=next(iter(source_hashes));c=cases[h];event['binding']='exact_typed_model_and_metadata_in_same_test' if mdsha else 'exact_detached_typed_model_in_same_test';event['input_sha256']=h
   c['provenance'].append({'kind':'executed_public_ascii','test_name':row['test'],'entry':row['entry'],'status':row['status'],'options_sha256':optsha,'output_sha256':outsha,'report_sha256':rsha,'capture_event':i,'binding':event['binding'],'model_debug_sha256':msha,'metadata_sha256':mdsha,'error':row.get('error')})
   if outsha:
    snap={'path':'runtime-capture/reference-outputs/'+outsha+'.txt','sha256':outsha,'kind':'generated_pinned_upstream_reference','options_sha256':optsha,'report_sha256':rsha}
    if snap not in c['expectation']['snapshots']:c['expectation']['snapshots'].append(snap)
  elif source_hashes:event['binding']='ambiguous_equivalent_prior_source_models';ambiguous.append(event)
  else:event['binding']='typed_model_without_observed_authored_source';detached.append(event)
  events.append(event);ascii_events.append((row,event))
capfile=PKG/'src/capability.rs';captext=capfile.read_text();capabilities={}
for cap in re.finditer(r'diagram_type: "([^"]+)".*?primary_projection: AsciiPrimaryProjection::(\w+)',captext,re.S):
 fid=familymap.get(cap.group(1),cap.group(1));capabilities[fid]={'upstream_family':cap.group(1),'primary_projection':cap.group(2),'source_line_start':captext.count('\n',0,cap.start())+1,'original_file_sha256':sha(capfile.read_bytes()),'evidence_scope':'declared primary family capability; not proof of per-render projection or semantic correctness'}
dump(A/'family-capabilities.json',capabilities)
for c in cases.values():
 e=c['expectation'];asc=[p for p in c['provenance'] if p['kind']=='executed_public_ascii'];positive=[p for p in asc if p['status']=='output'];prs=[p for p in c['provenance'] if p['kind']=='executed_public_parse']
 projections=[]
 for p in positive:
  report=reports.get(p['report_sha256'],'');m=re.search(r'projection: (Diagrammatic|StructuredText)',report)
  kind=m.group(1) if m else ('StructuredTextFamilyDeclared' if capabilities.get(c['family'],{}).get('primary_projection')=='StructuredText' else 'UnreportedText');outcome=re.search(r'outcome: (Primary|WideAllowed|Fallback|Empty)',report)
  if outcome and outcome.group(1) in ('Fallback','Empty'):kind+='_'+outcome.group(1)
  projections.append(kind)
 e['upstream_observed_output_kinds']=dict(collections.Counter(projections));e['declared_family_projection']=capabilities.get(c['family']);e['projection_policy']='Only observed primary Diagrammatic reports count as drawing. Eight structured-only families use declared family capabilities when String API omits reports; other String results remain projection-unresolved.'
 if 'Diagrammatic' in projections:e['classification']='drawing'
 elif 'UnreportedText' in projections:e['classification']='terminal_output_unreported_projection'
 elif any(p.startswith('StructuredText') for p in projections):e['classification']='structured_text'
 elif projections:e['classification']='fallback_or_empty_output'
 elif asc:e['classification']='foreign_policy_or_render_error'
 elif all(p['status']=='error' for p in prs):e['classification']='parse_error'
 else:e['classification']='parse_only_no_unambiguous_ascii_binding'
 e['semantic_obligation_source']='Pinned parser compatibility model + exact original assertions; returning a reference text or source fallback is not semantic drawing proof.'
 e['policy_difference']='Resource/cancellation/viewport errors apply to original foreign options; unsafe control text and width policies require explicit Loom decisions.'
# Resolve every lexical candidate against actual fully formatted public input arguments.
inv=json.loads((OLD/'inline-inventory.json').read_text());resolution=[];inputs_by_hash={h:c['input'] for h,c in cases.items()}
def template_regex(s):
 out=[];i=0
 while i<len(s):
  if s[i:i+2]=='{{':out.append(re.escape('{'));i+=2
  elif s[i:i+2]=='}}':out.append(re.escape('}'));i+=2
  elif s[i]=='{':
   end=s.find('}',i+1)
   if end<0:out.append(re.escape(s[i:]));break
   out.append('.*?');i=end+1
  else:out.append(re.escape(s[i]));i+=1
 return re.compile('^'+''.join(out)+'$',re.S)
for cand in inv['candidates']:
 template=(OLD/cand['candidate_path']).read_bytes().decode();origin=cand['source'];fn=None
 if origin in sources:
  p,t,fs=sources[origin];pos=len(t.encode()[:cand['source_byte_offset']].decode())
  enclosed=[f for f in fs if f['start']<=pos<f['end']]
  if enclosed:fn=max(enclosed,key=lambda f:f['start'])
 compatible=[]
 for row,event in parse_events:
  same_test=bool(fn and row['test'].split('::')[-1]==fn['name'])
  same_helper=bool(fn and orig_path(row['path'])==origin and fn['line_start']<=row['line']<=fn['line_end'])
  if same_test or same_helper:compatible.append(event['input_sha256'])
 exact=[cand['selected_sha256']] if cand['selected_sha256'] in inputs_by_hash else []
 dynamic=cand['needs_dynamic_expansion'];matched=[];method=None
 if dynamic:
  pat=template_regex(template);matched=sorted({h for h in compatible if pat.fullmatch(inputs_by_hash[h])});method='format_template_full_match_in_same_test_or_helper'
 else:
  matched=exact;method='exact_input_utf8_sha256'
  if not matched:
   matched=sorted({h for h in compatible if template.strip()==inputs_by_hash[h]});method='explicit_trimmed_argument_variant_same_test_or_helper'
 if not matched:
  partial=sorted({h for h in compatible if template and template in inputs_by_hash[h]})
  if partial:matched=partial;method='literal_fragment_in_observed_complete_argument_same_test_or_helper'
 non_source_roles={'c6508a5cc073ce11':'expected_structured_output_for_typed_model_only_test','8834ac147c862da5':'assertion_failure_message','6d5dc13ba8063ca2':'assertion_failure_message','4d62a774db52ae2f':'expected_structured_output','740cfd81b8c0c014':'expected_structured_output','3bf4a8b31541a0f0':'expected_structured_output_for_typed_model_only_test'}
 resolution.append({'candidate_role':non_source_roles.get(cand['id'],'authored_source_or_template'),'candidate_id':cand['id'],'source':origin,'source_line':cand['source_line'],'source_expression_sha256':cand['source_expression_sha256'],'needs_dynamic_expansion':dynamic,'enclosing_function':fn['name'] if fn else None,'status':'observed_complete_public_input' if matched else ('audited_non_source_literal' if cand['id'] in non_source_roles else 'unresolved_or_private_model_only'),'method':method if matched else None,'runtime_input_sha256':matched,'compatible_test_input_count':len(set(compatible))})
for h,raw in models.items():p=A/'oracle-models'/(h+'.json');p.parent.mkdir(exist_ok=True);p.write_bytes(raw)
for h,raw in outputs.items():p=A/'reference-outputs'/(h+'.txt');p.parent.mkdir(exist_ok=True);p.write_bytes(raw)
dump(A/'options.json',options);dump(A/'metadata.json',metadata);dump(A/'reports.json',reports);dump(A/'invocations.json',{'schema_version':1,'events':events});dump(A/'candidate-resolution.json',{'schema_version':1,'candidates':resolution});dump(A/'typed-model-only.json',{'schema_version':1,'source_free_or_modified_model_invocations':detached,'ambiguous_equivalent_source_invocations':ambiguous})
for filename in ['LICENSE-MIT','LICENSE-APACHE','THIRD_PARTY_NOTICES.md']:
 src=UP/filename
 if src.exists():shutil.copyfile(src,A/filename)
# Never alter the original 100-case corpus or its lock.
old=json.loads((OLD/'cases.json').read_text());old_hashes={c['sha256'] for c in old['cases']}
for h,c in cases.items():c['existing_merman_fixture_identity']=h in old_hashes
log=(S/'merman-observer-test-exact.log').read_text();summaries=re.findall(r'test result: (\w+)\. (\d+) passed; (\d+) failed; (\d+) ignored;',log);counts={'suites':len(summaries),'passed':sum(int(x[1]) for x in summaries),'failed':sum(int(x[2]) for x in summaries),'ignored':sum(int(x[3]) for x in summaries),'raw_events':len(rows),'parse_invocations':len(parse_events),'exact_source_inputs':len(cases),'ascii_invocations':len(ascii_events),'source_free_or_modified_ascii_invocations':len(detached),'ambiguous_ascii_invocations':len(ambiguous),'classifications':dict(collections.Counter(c['expectation']['classification'] for c in cases.values())),'families':dict(collections.Counter(c['family'] for c in cases.values())),'source_candidates':len(resolution),'static_candidate_resolutions':dict(collections.Counter(r['status'] for r in resolution if not r['needs_dynamic_expansion'])),'dynamic_candidate_resolutions':dict(collections.Counter(r['status'] for r in resolution if r['needs_dynamic_expansion'])),'old_fixture_identity_overlap':sum(h in old_hashes for h in cases),'typed_models':len(models),'reference_outputs':len(outputs),'render_options':len(options),'metadata_variants':len(metadata),'assertion_slices':len(assertions)}
new={'schema_version':1,'upstream':{'repository':'https://github.com/Latias94/merman','tag':'v0.9.1','commit':'f6202f702dbde0a75049b1a62b3124feae709b3d','license':'MIT OR Apache-2.0'},'scope':{'source':'Actual public parser and ASCII invocations in all pinned merman-ascii --lib --tests with all-diagrams','identity':'Exact UTF-8 SHA256; no normalized or fabricated input','native_mapping':'Pending independent review; these are upstream observations, not Loom semantic passes','original_frozen_corpus':'Unchanged; additive candidate handoff only'},'counts':counts,'cases':list(cases.values())};dump(DEST/'runtime-cases.json',new)
files=[{'path':str(p.relative_to(DEST)),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in sorted(DEST.rglob('*')) if p.is_file() and p.name!='provenance.json']
proof={'schema_version':1,'upstream':new['upstream'],'counts':counts,'suite_command':'cargo test --locked --manifest-path <pinned scratch>/Cargo.toml -p merman-ascii --features all-diagrams --lib --tests -- --test-threads=4','raw_capture_sha256':sha((S/'merman-executed-inputs-exact.jsonl').read_bytes()),'suite_log_sha256':sha((S/'merman-observer-test-exact.log').read_bytes()),'suite_summaries':[{'status':s,'passed':int(p),'failed':int(f),'ignored':int(i)} for s,p,f,i in summaries],'binding_policy':'Exact typed-model Debug SHA256 and bytes, same process and upstream test, plus exact metadata for parsed render. Equivalent prior sources remain ambiguous; modified/detached source-free models receive no invented source. Ascii-only public models are not source cases.','source_sha256_check':'SHA is derived from the complete exact model Debug bytes captured at both public entrypoints; noncryptographic preliminary hashes are unused.','files':files,'assertion_slice_provenance':list(assertions.values())};dump(A/'provenance.json',proof);print(json.dumps(counts,indent=2))
