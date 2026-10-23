#!/usr/bin/env python3
"""Prepare a deterministic, reviewable data-only Merman corpus promotion."""
from pathlib import Path
import argparse,collections,copy,hashlib,json,re,shutil,unicodedata
EXPECTED_BASE='12b6085a9ff8926c2822dd7db4441c4938d96f194addb839b3bb278bc9b1fe87'
EXPECTED_RUNTIME='fbd5c52af432d222848b41a8cbd703093ed2280f3585e410621f779e641ddbc8'
EXPECTED_CAPABILITIES='ad7200afd3117832110f168e60bb40e6fa47c729cb9ed0c4ca7c61df654e1bae'
def sha(raw):return hashlib.sha256(raw).hexdigest()
def canonical(x):return json.dumps(x,ensure_ascii=False,sort_keys=True,separators=(',',':'))
def dump(path,x):path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(x,ensure_ascii=False,sort_keys=True,indent=2)+'\n')
def load(path,expected):
 raw=path.read_bytes()
 if sha(raw)!=expected:raise ValueError(f'Input hash mismatch: {path}')
 return json.loads(raw)
def family(name):return {'gitGraph':'gitgraph','treeView':'treeview','quadrantChart':'quadrant','quadrantchart':'quadrant'}.get(name,name)
def dedup_append(target,values):
 seen={canonical(x) for x in target}
 for v in values:
  key=canonical(v)
  if key not in seen:target.append(copy.deepcopy(v));seen.add(key)
def observed_category(kinds,original):
 if kinds.get('Diagrammatic'):return 'primary_diagrammatic_report'
 if kinds.get('StructuredText'):return 'primary_structured_text_report'
 if kinds.get('UnreportedText'):return 'string_output_projection_unresolved'
 if kinds.get('StructuredTextFamilyDeclared'):return 'string_output_declared_structured_family'
 if kinds and all(k.endswith('_Fallback') for k in kinds):return 'fallback_only'
 if kinds and all(k.endswith('_Empty') for k in kinds):return 'empty_only'
 if kinds:return 'mixed_or_unclassified_output_kinds'
 return {'parse_only_no_unambiguous_ascii_binding':'parsed_source_no_unambiguous_ascii_binding','foreign_policy_or_render_error':'foreign_policy_or_render_error','parse_error':'parse_error'}.get(original,'unclassified_no_output')
def policy(s):
 control=sorted({f'U+{ord(c):04X}' for c in s if (ord(c)<32 and c not in '\r\n\t') or 0x7f<=ord(c)<=0x9f or 0x202a<=ord(c)<=0x202e or 0x2066<=ord(c)<=0x2069})
 combining=sorted({f'U+{ord(c):04X}' for c in s if unicodedata.combining(c)})
 return {'terminal_control_or_bidi_codepoints':control,'combining_codepoints':combining,'scope':'Valid public text inputs remain in the syntax denominator; safe disclosure and grapheme-width policy require separate native predicates, never raw control passthrough or a silent fallback pass.'}
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--base',type=Path,required=True);ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--capabilities',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);args=ap.parse_args()
 base=load(args.base,EXPECTED_BASE);runtime=load(args.runtime,EXPECTED_RUNTIME);caps=load(args.capabilities,EXPECTED_CAPABILITIES)
 if caps['package']['version']!='0.9.1':raise ValueError('Unexpected capability package')
 capmap={family(x['family']):x for x in caps['ascii']['families']}
 args.out.mkdir(parents=True,exist_ok=True);merged=copy.deepcopy(base);old_by_sha={c['sha256']:c for c in merged['cases']};runtime_by_sha={c['sha256']:c for c in runtime['cases']}
 if len(runtime_by_sha)!=1632 or not set(old_by_sha)<=set(runtime_by_sha):raise ValueError('Unexpected source identity overlap')
 counts=collections.Counter();observation_counts=collections.Counter();source_counts=collections.Counter();family_counts=collections.Counter();policy_counts=collections.Counter();new_cases=[];adjudications=[]
 for h,r in sorted(runtime_by_sha.items()):
  if sha(r['input'].encode())!=h:raise ValueError('Input identity mismatch')
  old=h in old_by_sha;c=old_by_sha[h] if old else copy.deepcopy(r);e=c['expectation'];rexp=r['expectation'];declared=copy.deepcopy(capmap.get(r['family']))
  if declared is None:raise ValueError(f'Family missing from public capability catalog: {r["family"]}')
  p=declared['primary_projection'];prs=[x for x in r['provenance'] if x['kind']=='executed_public_parse'];parsed=any(x['status']=='parsed' for x in prs)
  if any(x['parse_options']!='ParseOptions { suppress_errors: false }' for x in prs):raise ValueError('Unexpected permissive parse mode')
  kinds=copy.deepcopy(rexp['upstream_observed_output_kinds']);observed=observed_category(kinds,rexp['classification'])
  source_scope='strict_public_source_accepted' if parsed else 'public_text_negative_probe'
  negative=None
  if not parsed:
   test_names={x['test_name'] for x in prs}
   if any(x.endswith('flowchart_parser_rejects_internal_icon_layout_shape_names') for x in test_names):negative='private_layout_shape_names_rejected_at_public_text_boundary'
   elif h=='ea820bd13ab63162e9a5d6b7a449f5da11f937eac1730b4c78b6775f56393dba':negative='documented_upstream_invalid_local_sequence_stress_fixture'
   elif r['input'].startswith('sequenceDiagram\n  participant (end) as end'):negative='documented_upstream_invalid_local_sequence_stress_fixture'
   else:raise ValueError(f'Unadjudicated parser failure {h}')
   target='error';obligation='reject_or_preserve_source_with_explicit_parse_error';counts['negative_inputs']+=1
  elif p=='diagrammatic':target='drawing';obligation='draw_source_semantics_with_node_shape_marker_group_order_value_preservation';counts['drawing_obligations']+=1
  elif p=='structured_text':target='structured_text';obligation='preserve_typed_semantics_in_structured_text_without_claiming_diagram_geometry';counts['structured_text_obligations']+=1
  elif p=='none':target='fallback';obligation='explicit_whole_source_fallback_for_upstream_unsupported_terminal_family';counts['correct_fallback_obligations']+=1
  else:raise ValueError(f'Unexpected primary projection {p}')
  if old:
   if e['classification']!=target:raise ValueError(f'Legacy expectation would change {h}')
   dedup_append(c['provenance'],r['provenance']);dedup_append(e.setdefault('snapshots',[]),rexp.get('snapshots',[]));dedup_append(e.setdefault('assertions',[]),rexp.get('assertions',[]))
  else:
   c['id']='merman/'+h[:20];e['classification']=target;new_cases.append(c);counts['additional_exact_inputs']+=1
   e.setdefault('contains',[]);e.setdefault('not_contains',[])
  # Keep original facts/models authoritative; runtime models become additional variants.
  e['runtime_semantic_model_variants']=copy.deepcopy(rexp.get('semantic_model_variants',[]))
  e['upstream_observation']={'original_runtime_classification':rexp['classification'],'observed_kind_counts':kinds,'category':observed,'declared_primary_family_projection':p,'source_binding':'Only exact model/config bindings from immutable runtime invocation ledger; ambiguous calls remain unbound.','interpretation':'Observation and declared native obligation are independent. String output and fallback are not evidence of diagrammatic rendering.'}
  e['native_requirement']={'classification':target,'required_behavior':obligation,'basis':'Strict public source admission plus pinned public CLI family capability; this is a declared test obligation, not an observed Loom or upstream rendering pass.','family_capability':declared,'native_semantic_verification':'pending'}
  c['public_source_scope']={'category':source_scope,'parser_entrypoint':'Engine::parse_diagram_for_render_model_sync','suppress_errors':False,'official_mermaid_validation':'Not independently rerun against Mermaid.js; strict source admission and explicit negative source assertions are the authorities for this import.','negative_probe_kind':negative}
  pol=policy(r['input'])
  if pol['terminal_control_or_bidi_codepoints'] or pol['combining_codepoints']:
   c['terminal_text_policy']=pol
   if pol['terminal_control_or_bidi_codepoints']:policy_counts['control_or_bidi_source_cases']+=1
   if pol['combining_codepoints']:policy_counts['combining_source_cases']+=1
  counts['total_exact_inputs']+=1;counts['legacy_ids_preserved']+=old;observation_counts[observed]+=1;source_counts[source_scope]+=1;family_counts[r['family']]+=1
  adjudications.append({'sha256':h,'case_id':c['id'],'legacy_identity':old,'family':r['family'],'observed_category':observed,'observed_kind_counts':kinds,'declared_primary_projection':p,'native_obligation':target,'source_scope':source_scope,'negative_probe_kind':negative})
 merged['cases'].extend(new_cases)
 # Check every original fact and ID remains unchanged; only append evidence or new fields.
 for original in base['cases']:
  candidate=old_by_sha[original['sha256']]
  if candidate['id']!=original['id']:raise ValueError('Legacy ID changed')
  for key,value in original['expectation'].items():
   if key=='snapshots':
    if candidate['expectation'][key][:len(value)]!=value:raise ValueError('Legacy snapshots changed')
   elif candidate['expectation'][key]!=value:raise ValueError(f'Legacy fact changed: {key}')
  if candidate['provenance'][:len(original['provenance'])]!=original['provenance']:raise ValueError('Legacy provenance changed')
 merged['runtime_scope']={'source':'Pinned complete merman-ascii runtime capture, merged by exact UTF-8 SHA256','original_fixture_scope_preserved':copy.deepcopy(base['scope']),'counts':dict(counts),'observation_counts':dict(observation_counts),'family_counts':dict(family_counts),'source_admission_counts':dict(source_counts),'terminal_text_policy_counts':dict(policy_counts),'private_denominator_exclusions':{'source_free_or_modified_typed_model_invocations':519,'lexical_non_source_literals':6,'note':'These are foreign-model API calls or expected output/assertion strings, never fabricated Mermaid inputs.170ambiguous ASCII invocations stay separate; their valid source inputs are retained without claiming observed rendering.'}}
 dump(args.out/'cases.json',merged);dump(args.out/'runtime-capture/promotion-obligations.json',{'schema_version':1,'cases':adjudications,'counts':dict(counts),'observation_counts':dict(observation_counts),'source_admission_counts':dict(source_counts),'terminal_text_policy_counts':dict(policy_counts)})
 shutil.copyfile(args.capabilities,args.out/'runtime-capture/full-public-capabilities.json')
 script_out=args.out/'maintenance/merge_runtime_cases.py';script_out.parent.mkdir(parents=True,exist_ok=True)
 if Path(__file__).resolve()!=script_out.resolve():shutil.copyfile(__file__,script_out)
 readme = """# Merman promotion candidate (review only)

This candidate merges 1,532 additional exact UTF-8 inputs into the frozen
100-case source corpus. All 100 original case IDs, source identities, expectation
facts, primary oracle references, and provenance prefixes remain unchanged.
Repeated source variants/options are merged by exact SHA256; no whitespace
normalization or typed-model-to-source synthesis occurs.

## Independently declared native requirements

The candidate has 1,632 source inputs: 1,605 diagrammatic-family drawing
obligations, 18 structured-family semantic-text obligations, 4 explicit fallback
obligations for upstream-unsupported terminal families, and 5 negative probes.
These requirements come from successful strict public source admission and the
complete public `merman-cli capabilities --json` document for 0.9.1. They are
requirements to implement/check in Loom, **not claims that these drawings pass**.
The complete capability document includes dynamically admitted Swimlane, which a
static source-table extractor omitted in the initial runtime handoff.

## Preserve observation separately from obligations

The actual runtime observations are 11 primary diagrammatic reports, 1 primary
structured-text report, 8 String results in declared structured-only families,
7 fallback-only results, 1,366 String results whose projection is unresolved,
213 source inputs without an unambiguous ASCII binding, 21 foreign policy/render
errors, and 5 parse errors. These categories total 1,632.

The immutable runtime handoff previously called 16 sources `structured_text`:
that combined the 1 primary report and 8 family-declared String results with
**7 explicit StructuredText_Fallback-only sources**. This candidate corrects the
interpretation in `upstream_observation.category`, preserving the original kind
counts and original classification for audit. Neither a fallback nor a String
result becomes observed diagrammatic evidence. Original runtime README/counts
remain historical extraction artifacts; this promotion note is the corrected
classification and must accompany them.

`expectation.native_requirement` states the desired Loom behavior.
`expectation.upstream_observation` retains measured projection/outcome and raw
classification. `declared_primary_family_projection` is a third, family-level
axis. These three concepts must not be substituted for one another.

## Public-source denominator and excluded private data

All 1,632 inputs were actual complete arguments at the public typed-parse API,
with `suppress_errors=false`. Of them 1,627 were strictly accepted; official
Mermaid.js admission was not independently rerun. Five are explicit public-text
negative probes: four internal layout shape names that must be rejected and one
documented upstream-invalid local sequence stress fixture. They remain a
separate negative denominator, rather than positive syntax coverage.

The 519 source-free/modified typed-model rendering **invocations** are not source
cases and receive no fabricated Mermaid text. Four expected-output literals and
two assertion-failure messages are likewise outside the source denominator.
The 170 ambiguous rendering invocations remain recorded without attributing
success to an arbitrary equivalent source. Two combining-mark source cases and
one bidi-control source case are explicit native width/disclosure policy cases;
they must not silently pass as ordinary label output or source fallback.

## Proposed promotion sequence

1. Independently review this candidate and the immutable runtime handoff.
2. Copy its existing runtime-capture assets unchanged into the vendor directory.
3. Apply only this candidate's cases.json, promotion-obligations.json,
   full-public-capabilities.json, PROMOTION.md, and maintenance merger.
4. Keep historical source manifests/locks as provenance; create a reviewed
   promotion inventory and only then regenerate the parent-owned root lock.
5. Run native JSON/text consumers. Normal tests need no foreign renderer,
   Python, Rust, or Node runtime.

This script writes scratch output only. It never modifies the real vendor tree,
root lock, production renderer, build files, or Git state. Re-running with the
same three input hashes yields identical output. Existing fixture assets and
runtime-capture models/assertions/references are shared rather than duplicated.
"""
 (args.out/'README.md').write_text(readme)
 (args.out/'runtime-capture/PROMOTION.md').write_text(readme)
 inventory={'schema_version':1,'status':'scratch_candidate_waiting_for_independent_review_and_atomic_promotion','source_hashes':{'legacy_cases':EXPECTED_BASE,'immutable_runtime_cases':EXPECTED_RUNTIME,'public_cli_capabilities':EXPECTED_CAPABILITIES},'counts':dict(counts),'upstream_observation_counts':dict(observation_counts),'source_admission_counts':dict(source_counts),'terminal_text_policy_counts':dict(policy_counts),'merge_invariants':['Preserve all100legacycaseIDsandalloriginalexpectationfacts','ExactsourceSHAidentityonly; merge100overlapsandappend1532newcases','Do not claim String or fallback observations as diagrammatic passes','Typed-model-only and lexical non-input data are outside source denominator','No production/runtime/foreign-language dependency in native tests'],'asset_promotion_plan':[{'action':'keep','path':'existing Merman vendor assets, original models/assertions/licenses, manifest and handoff-lock','note':'Original handoff provenance remains immutable; parent must create a separate reviewed promotion lock.'},{'action':'copy_unmodified','from':'merman-runtime-import/runtime-capture/','to':'merman/runtime-capture/','source_provenance_sha256':'285ded2dce74ca61308eda7800a3f02d2d9ae8a7766425eb323e2e6b7441108b'},{'action':'overlay_candidate_files','paths':['cases.json','runtime-capture/promotion-obligations.json','runtime-capture/full-public-capabilities.json','runtime-capture/PROMOTION.md','maintenance/merge_runtime_cases.py'],'note':'Review before any copy; this script never edits the real vendor or root inventory.'}],'files':[{'path':str(p.relative_to(args.out)),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in sorted(args.out.rglob('*')) if p.is_file() and p.name not in ['promotion-inventory.json','README.md']]}
 dump(args.out/'promotion-inventory.json',inventory);print(json.dumps({'counts':dict(counts),'observed':dict(observation_counts),'source':dict(source_counts),'policy':dict(policy_counts),'cases_sha256':sha((args.out/'cases.json').read_bytes())},indent=2))
if __name__=='__main__':main()
