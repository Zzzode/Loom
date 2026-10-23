from pathlib import Path
import re,sys
UP=Path(sys.argv[1]).resolve()
HELPER=r'''
fn loom_observer_hash(value: &impl std::fmt::Debug) -> String {
    use std::hash::{Hash, Hasher};
    let mut hash = std::collections::hash_map::DefaultHasher::new();
    format!("{value:?}").hash(&mut hash);
    format!("{:016x}", hash.finish())
}
fn loom_observer_write(value: serde_json::Value) {
    if let Ok(path) = std::env::var("MERMAN_OBSERVER_PATH") {
        use std::io::Write;
        static LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());
        let _guard = LOCK.lock().unwrap();
        let line = format!("{}\n", value);
        let mut file = std::fs::OpenOptions::new().create(true).append(true).open(path).unwrap();
        file.write_all(line.as_bytes()).unwrap();
    }
}
'''
core=UP/'crates/merman-core/src/lib.rs';s=core.read_text();old='''    pub fn parse_diagram_for_render_model_sync(
        &self,
        text: &str,
        options: ParseOptions,
    ) -> Result<Option<ParsedDiagramRender>> {
        parse_pipeline::ParsePipeline::detect(self, text, options).parse_render_model()
    }'''
new='''    #[track_caller]
    pub fn parse_diagram_for_render_model_sync(
        &self,
        text: &str,
        options: ParseOptions,
    ) -> Result<Option<ParsedDiagramRender>> {
        let result = parse_pipeline::ParsePipeline::detect(self, text, options).parse_render_model();
        if std::env::var_os("MERMAN_OBSERVER_PATH").is_some() {
            let location = std::panic::Location::caller();
            let parsed = result.as_ref().ok().and_then(|p| p.as_ref());
            let model = parsed.and_then(|p| p.model().compatibility_json(p.metadata()).ok());
            loom_observer_write(serde_json::json!({
                "event": "parse_return", "entry": "parse_diagram_for_render_model_sync",
                "pid": std::process::id(), "test": std::thread::current().name().unwrap_or("unnamed"),
                "path": location.file(), "line": location.line(), "input": text,
                "parse_options": format!("{options:?}"),
                "model_debug_hash": parsed.map(|p| loom_observer_hash(p.model())),
                "model_debug": parsed.map(|p| format!("{:?}",p.model())),
                "metadata_debug": parsed.map(|p| format!("{:?}",p.metadata())),
                "upstream_semantic_model": model.as_ref(),
                "status": if result.is_err() { "error" } else if parsed.is_none() { "none" } else { "parsed" },
                "error": result.as_ref().err().map(|e| e.to_string())
            }));
        }
        result
    }'''
assert s.count(old)==1;s=s.replace(old,new);s+='\n'+HELPER;core.write_text(s)
# A lightweight string/comment mask locates function braces without rewriting their content.
TOKEN=re.compile(r'r(?P<h>#{0,16})".*?"(?P=h)|b?"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\\n])\'|//[^\n]*|/\*.*?\*/',re.S)
def mask(t):return TOKEN.sub(lambda m:''.join('\n' if c=='\n' else ' ' for c in m.group()),t)
p=UP/'crates/merman-ascii/src/lib.rs';s=p.read_text()
for name in ['render_model','render_model_report','render_parsed','render_parsed_report']:
 m=re.search(r'    pub fn '+name+r'\(',s);a=s.index('{',m.end());masked=mask(s);depth=1;b=a+1
 while depth:
  depth+=(masked[b]=='{')-(masked[b]=='}');b+=1
 body=s[a+1:b-1];parsed='parsed' in name;report=name.endswith('report');expr='parsed.model()' if parsed else 'model'
 out='o.text.clone()' if report else 'o.clone()'
 obs='''
        let result = (|| { BODY\n        })();
        if std::env::var_os("MERMAN_OBSERVER_PATH").is_some() {
            let location = std::panic::Location::caller();
            loom_observer_write(serde_json::json!({
                "event": "ascii_return", "entry": "NAME",
                "pid": std::process::id(), "test": std::thread::current().name().unwrap_or("unnamed"),
                "path": location.file(), "line": location.line(),
                "model_debug_hash": loom_observer_hash(EXPR),
                "model_debug": format!("{:?}",EXPR),
                "metadata_debug": METADATA,
                "options_debug": format!("{:?}", self.options),
                "status": if result.is_ok() { "output" } else { "error" },
                "output": result.as_ref().ok().map(|o| OUT),
                "error": result.as_ref().err().map(|e| e.to_string()),
                "report_debug": REPORT
            }));
        }
        result
    '''.replace('BODY',body).replace('NAME',name).replace('EXPR',expr).replace('OUT',out).replace('METADATA','Some(format!("{:?}",parsed.metadata()))' if parsed else 'Option::<String>::None').replace('REPORT','result.as_ref().ok().map(|o| format!("{o:?}"))' if report else 'Option::<String>::None')
 s=s[:m.start()]+'    #[track_caller]\n'+s[m.start():a+1]+obs+s[b-1:]
s+='\n'+HELPER;p.write_text(s)
