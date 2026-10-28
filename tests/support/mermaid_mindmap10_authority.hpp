// Independent RFC 0030 Mindmap10 source authority and pure geometry oracle.
// Callers own raw asset hashing, document lifetimes and public rendering evidence.
#pragma once
#include <ftxui/screen/string.hpp>
import std;
import loom.serdes.json;
import loom.crypto.crypto;

namespace loom::test::mermaid_mindmap10 {
using JsonVal = loom::utils::json::JsonVal;
inline constexpr std::string_view kFactsSha = "fa675abb49ce3de96e082b17cad12467296f06db80af28c9edafa8d4b6815627";
inline constexpr std::string_view kFactsRelativePath = "mermaid_native/mindmap10_complete_source_proposal.v1.json";
struct Work {
    std::size_t left = 64000000;
    void spend(std::size_t n) {
        if (n > left) throw std::runtime_error("resource: Mindmap10 Work exhausted");
        left -= n;
    }
};
struct Grid {
    int width, height;
    std::vector<std::string> cells;
    std::string_view at(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return {};
        const auto i = static_cast<std::size_t>(y)*static_cast<std::size_t>(width)+static_cast<std::size_t>(x);
        return i < cells.size() ? std::string_view(cells[i]) : std::string_view{};
    }
};
struct Node {
    int ordinal = -1, parent = -1;
    std::string caption, shape;
    std::optional<std::string> authored_id;
    std::vector<std::string> rows, joiners;
};
struct Model { std::vector<Node> nodes; };
struct MutationSummary { std::size_t semantic = 0, physical = 0; std::vector<std::string> errors; };

namespace detail {
inline void require(bool ok, std::string_view message) {
    if (!ok) throw std::runtime_error(std::string(message));
}
inline void pay(Work& w, std::size_t n, std::size_t scale, std::size_t fixed = 0) {
    if (fixed > w.left || (scale && n > (w.left-fixed)/scale))
        throw std::runtime_error("resource: Mindmap10 Work exhausted");
    w.spend(n*scale+fixed);
}
inline std::string_view text(JsonVal v, Work& w, std::size_t limit = 65536) {
    w.spend(8);
    require(v.is_str(), "protocol: Mindmap10 string type");
    require(v.as_str().size() <= limit, "protocol: Mindmap10 string bound");
    pay(w,v.as_str().size(),1);
    return v.as_str();
}
inline int integer(JsonVal v, Work& w) {
    w.spend(8);
    require(v.is_int() && v.as_int() >= -1 && v.as_int() <= 65536, "protocol: Mindmap10 integer type/range");
    return static_cast<int>(v.as_int());
}
inline void keys(JsonVal v, std::initializer_list<std::string_view> wanted, Work& w) {
    w.spend(128);
    require(v.is_obj() && v.size() == wanted.size() && wanted.size() <= 32, "protocol: Mindmap10 object key set");
    std::array<bool,32> seen{};
    v.iter_obj([&](JsonVal k, JsonVal) {
        const auto name = text(k,w,256);
        w.spend(wanted.size());
        const auto it = std::ranges::find(wanted,name);
        require(it != wanted.end(), "protocol: Mindmap10 unknown key");
        const auto i = static_cast<std::size_t>(it-wanted.begin());
        require(!seen[i], "protocol: Mindmap10 duplicate key"); seen[i] = true;
    });
}
inline std::string hash(std::string_view s, Work& w) {
    pay(w,s.size(),8,256);
    return loom::utils::crypto::sha256(s);
}
inline void append(std::string& out, std::string_view s, Work& w) {
    pay(w,s.size(),2,8); out.append(s);
}
inline std::string quoted(std::string_view s, Work& w) {
    pay(w,s.size(),8,64);
    std::string out; out.reserve(s.size()*6+2); out += '"';
    constexpr std::string_view hex = "0123456789abcdef";
    for (const unsigned char c : s) {
        w.spend(1);
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 32) { out += "\\u00"; out += hex[c>>4]; out += hex[c&15]; }
            else out += static_cast<char>(c);
        }
    }
    out += '"'; return out;
}
inline std::string encode(JsonVal v, Work& w, int depth) {
    w.spend(64);
    require(v.valid() && depth <= 32, "protocol: Mindmap10 canonical value/depth");
    if (v.is_str()) return quoted(text(v,w),w);
    if (v.is_int()) return std::to_string(v.as_int());
    if (v.is_bool()) return v.as_bool() ? "true" : "false";
    if (v.is_null()) return "null";
    require(v.is_obj() || v.is_arr(), "protocol: Mindmap10 canonical unsupported number");
    require(v.size() <= 4096, "protocol: Mindmap10 canonical container bound");
    std::string out;
    if (v.is_arr()) {
        append(out,"[",w);
        for (std::size_t i=0; i<v.size(); ++i) {
            w.spend(i+1); if (i) append(out,",",w);
            append(out,encode(v.at(i),w,depth+1),w);
        }
        append(out,"]",w);
    } else {
        pay(w,v.size(),sizeof(std::pair<std::string_view,JsonVal>)+64);
        std::vector<std::pair<std::string_view,JsonVal>> entries; entries.reserve(v.size());
        v.iter_obj([&](JsonVal k,JsonVal child) { entries.emplace_back(text(k,w,256),child); });
        pay(w,entries.size(),entries.size()+1);
        std::ranges::sort(entries,{},&std::pair<std::string_view,JsonVal>::first);
        append(out,"{",w);
        for (std::size_t i=0; i<entries.size(); ++i) {
            require(i==0 || entries[i-1].first!=entries[i].first, "protocol: Mindmap10 canonical duplicate key");
            if (i) append(out,",",w);
            append(out,quoted(entries[i].first,w),w); append(out,":",w);
            append(out,encode(entries[i].second,w,depth+1),w);
        }
        append(out,"}",w);
    }
    return out;
}
// FTXUI's empty continuation slots are retained, never filtered or compacted.
inline std::vector<std::string> slots(std::string_view s, Work& w) {
    require(s.size() <= 1024, "model: Mindmap10 label byte bound");
    pay(w,s.size(),8*sizeof(std::string)+64,256);
    // Reject malformed UTF-8 independently of the text helper's replacement policy.
    for (std::size_t i=0;i<s.size();) {
        w.spend(8); const auto lead=static_cast<unsigned char>(s[i]);
        std::size_t count=1; std::uint32_t cp=lead;
        if (lead>=0xc2 && lead<=0xdf) { count=2; cp=lead&31; }
        else if (lead>=0xe0 && lead<=0xef) { count=3; cp=lead&15; }
        else if (lead>=0xf0 && lead<=0xf4) { count=4; cp=lead&7; }
        else require(lead<128, "model: Mindmap10 malformed UTF-8 lead");
        require(count<=s.size()-i, "model: Mindmap10 truncated UTF-8");
        for (std::size_t j=1;j<count;++j) {
            w.spend(2); const auto c=static_cast<unsigned char>(s[i+j]);
            require((c&0xc0)==0x80, "model: Mindmap10 malformed UTF-8 continuation"); cp=(cp<<6)|(c&63);
        }
        require((count==1 || cp>=(count==2?0x80u:count==3?0x800u:0x10000u)) && cp<=0x10ffff && !(cp>=0xd800 && cp<=0xdfff), "model: Mindmap10 UTF-8 scalar range");
        i+=count;
    }
    std::string bytes(s);
    auto glyphs = ftxui::Utf8ToGlyphs(bytes);
    std::string restored;
    for (const auto& g:glyphs) { w.spend(g.size()+1); restored += g; }
    require(restored==s && ftxui::string_width(bytes)==static_cast<int>(glyphs.size()), "model: Mindmap10 glyph round trip");
    return glyphs;
}
struct Projection { std::vector<std::string> rows, joiners; };
inline Projection wrap(std::string_view caption, Work& w) {
    pay(w,caption.size(),16*sizeof(std::string)+128,1024);
    Projection p; std::string row, pending; int used=0;
    auto flush = [&](std::string_view joiner) {
        w.spend(row.size()+64);
        if (!p.rows.empty()) p.joiners.emplace_back(pending);
        p.rows.push_back(row); row.clear(); used=0; pending=joiner;
    };
    std::size_t start=0;
    while (start<caption.size()) {
        w.spend(1);
        const auto end=caption.find(' ',start);
        const auto word=caption.substr(start,end==std::string_view::npos?caption.size()-start:end-start);
        const auto g=slots(word,w); const int width=static_cast<int>(g.size());
        if (!row.empty()) {
            if (width>32 || used+1+width>32) flush(" ");
            else { row += ' '; ++used; }
        }
        for (std::size_t i=0; i<g.size();) {
            w.spend(8);
            require(!g[i].empty(), "model: Mindmap10 detached wide continuation");
            const std::size_t count = i+1<g.size() && g[i+1].empty()?2:1;
            if (used+static_cast<int>(count)>32) flush("");
            row += g[i]; used += static_cast<int>(count); i += count;
        }
        if (end==std::string_view::npos) break;
        start=end+1;
    }
    if (!row.empty()) flush("");
    return p;
}
inline std::vector<std::string> strings(JsonVal v, Work& w) {
    require(v.is_arr() && v.size()<=256, "protocol: Mindmap10 row array");
    pay(w,v.size(),sizeof(std::string)+64);
    std::vector<std::string> result; result.reserve(v.size());
    v.iter([&](JsonVal s) { const auto t=text(s,w,1024); pay(w,t.size(),2); result.emplace_back(t); });
    return result;
}
inline int index(const Model& m, int id, Work& w) {
    for (std::size_t i=0;i<m.nodes.size();++i) { w.spend(1); if (m.nodes[i].ordinal==id) return static_cast<int>(i); }
    return -1;
}
} // namespace detail

inline std::string canonical(JsonVal v, Work& w) { return detail::encode(v,w,0); }

inline std::vector<std::string> validate_model(const Model& m, Work& w) {
    using namespace detail;
    // Domain failures return diagnostics; resource exhaustion always escapes.
    auto check = [&] {
        w.spend(2048);
        require(!m.nodes.empty() && m.nodes.size()<=64, "model: Mindmap10 node count");
        std::array<int,64> ancestors{}; std::size_t depth=0;
        for (std::size_t i=0;i<m.nodes.size();++i) {
            w.spend(128); const auto& n=m.nodes[i];
            require(n.ordinal>=0 && n.ordinal<64 && index(m,n.ordinal,w)==static_cast<int>(i), "model: Mindmap10 unique source ordinal");
            require((i==0 && n.parent==-1) || (i>0 && n.parent>=0), "model: Mindmap10 one root");
            if (i) {
                while (depth && ancestors[depth-1]!=n.parent) { w.spend(1); --depth; }
                require(depth>0, "model: Mindmap10 parent/open preorder ancestor");
            }
            require(depth<16, "model: Mindmap10 depth bound"); ancestors[depth++]=n.ordinal;
            require(!n.caption.empty() && n.caption.size()<=1024 && n.caption.front()!=' ' && n.caption.back()!=' ' && n.caption.find("  ")==std::string::npos, "model: Mindmap10 plain caption spaces");
            for (unsigned char c:n.caption) { w.spend(1); require(c>=32 && c!=127, "model: Mindmap10 caption controls"); }
            const auto glyphs=slots(n.caption,w);
            require(!glyphs.empty() && glyphs.size()<=256, "model: Mindmap10 caption cell bound");
            require(n.shape=="default" || (n.shape=="square" && m.nodes.size()==1), "model: Mindmap10 closed shapes");
            require(n.shape=="square" ? n.authored_id==std::optional<std::string>("root") : !n.authored_id, "model: Mindmap10 paired shape/authored ID");
            require(!n.rows.empty() && n.rows.size()<=256 && n.joiners.size()+1==n.rows.size(), "model: Mindmap10 projection cardinality");
            pay(w,n.caption.size(),4,1024); std::string restored;
            for (std::size_t j=0;j<n.rows.size();++j) {
                w.spend(32); require(!n.rows[j].empty(), "model: Mindmap10 empty caption row");
                if (j) { require(n.joiners[j-1].empty() || n.joiners[j-1]==" ", "model: Mindmap10 row joiner domain"); restored+=n.joiners[j-1]; }
                require(restored.size()+n.rows[j].size()<=1024, "model: Mindmap10 reconstruction bound"); restored+=n.rows[j];
            }
            require(restored==n.caption, "model: Mindmap10 exact row-joiner reconstruction");
            if (n.shape=="default") require(n.rows.size()==1 && n.rows[0]==n.caption, "model: Mindmap10 unwrapped default caption");
            else { const auto p=wrap(n.caption,w); require(p.rows==n.rows && p.joiners==n.joiners, "model: Mindmap10 independent greedy square projection"); }
        }
    };
    try { check(); return {}; }
    catch (const std::runtime_error& e) {
        if (std::string_view(e.what()).starts_with("resource:")) throw;
        w.spend(512); return {e.what()};
    }
}

namespace detail {
struct Pin { int global; std::string_view path, row_sha, manifest_sha; };
inline constexpr std::array<Pin,10> pins{{
{1296,"mindmap/nightly-check/a-root-with-a-shape.mmd","e724d7fe797af1f2967af3a7b580ddd8d0edc0c0d718fbf3cdf0b991762ae907","84a033c7dd93120b43597133535fea223137fd6feadc6896bebefda6c6601b6f"},
{1298,"mindmap/nightly-check/a-root-with-wrapping-text-and-a-shape.mmd","0138968621e60f20bd1f799f7169fe6821269f437cce604b0db5789e80c03749","93725381101040e0e4f3260ff1a8c6ee940f14f720af95da0f5d69bd474f7a89"},
{1299,"mindmap/nightly-check/a-root-with-wrapping-text-and-long-words-that-exceed-width.mmd","a1ddfd62ad9264d537909607a6bd36e25407fe829f10febdcb93863768eaa465","fc1a412720c1d907c05d64d9e5d446f22d4892f1d021f01634a85a2a02c07bda"},
{1300,"mindmap/nightly-check/adding-children.mmd","0b82a9d81c27ccfef45e9211a58cda7e290b1760b4beb6a670a4dfa6a05729f2","cfee3a301cde50f907b0acdf9d5514e242468a13b4d3f023ddd652299d1bb47c"},
{1301,"mindmap/nightly-check/adding-grand-children.mmd","046e697c3067f7ae60acda07cdae4c7fe1cdd4b6258de89373befc8f1f1c6710","6d123ff46b66b1b46cd2933a0f6ef0bdde0753bad3416251c80e5d0206b1c3f1"},
{1312,"mindmap/nightly-check/branches.mmd","cfb1fbef28ac2d7dece0d7c864f9bfce21f9846ebc496b2258f2e84e80d3956b","e2efc57b22d37cc1f9b24c5e9f4061408df27b37a28d80ddc9962c40a2bde421"},
{1314,"mindmap/nightly-check/default-shape.mmd","576efc65597bed05af9d7808cd45109fcaf5987527ff2f299374d9b38f50f5dc","788aa28be0811341816be528fa71b366c619ac7e6ee46a7923d18efa42492867"},
{1315,"mindmap/nightly-check/level-2-nodes-exceeding-11.mmd","f48efea0d2f9eab5ba13cfe874f320bd4d1a2941615293ac04c9a05bac458c09","3ac1c1a80c81e37900d1fb7bf4ee74872d5d93c6b16ea6c73b5a390793c297b9"},
{1316,"mindmap/nightly-check/only-a-root.mmd","eda120fd4b0d2ca7a362747e97fb05f1bf9d8224c1b65e40ef7f9d29f2a1dee5","b1130825d6bddfd4a2b43b6407d3e3dabe76214dfabeb4e74dd98501b32be959"},
{1328,"mindmap/pr-check/simple-mindmap.mmd","9b9d47559954a6196b84cd0bc8df2b8897ce17046713cfe532692e9a0e22db35","cf967b4de86d896e638c478883b52fc6daa14a84f03db1464de40e5951c32816"},
}};
} // namespace detail

inline Model bind(JsonVal doc, JsonVal row, std::string_view source, JsonVal entireManifest, Work& w) {
    using namespace detail;
    keys(doc,{"schema","review_status","qualification","source_lock_sha256","complete_originals","nodes","parent_relations","cases","canonical_model_encoding","admission_status","terminal_adaptation_status","source_lock_encoding","review"},w);
    require(text(doc.get("schema"),w)=="loom_mindmap10_source_proposal/v1" && text(doc.get("review_status"),w)=="source_design_reviewed_no_runtime_admission", "protocol: Mindmap10 source-only schema/status");
    require(integer(doc.get("complete_originals"),w)==10 && integer(doc.get("nodes"),w)==45 && integer(doc.get("parent_relations"),w)==35, "protocol: Mindmap10 fixed fact totals");
    const auto review=doc.get("review");
    keys(review,{"date","reviewer","verdict","reviewed_proposal_sha256","scope"},w);
    require(text(review.get("reviewer"),w)=="agent:Parfit" && text(review.get("verdict"),w)=="approved_bounded_source_design_and_prr" && text(review.get("reviewed_proposal_sha256"),w)=="8307a8358dfe0574797532f174c2c94605277d6bcbb8b0c0e64b7a969aaa75eb", "fact-review: Mindmap10 approved source/design/PRR only");
    keys(row,{"path","source_sha256","complete_model","complete_model_sha256","source_witnesses","manifest_global_ordinal","source_git_blob","source_bytes","required_good_first_mutations","terminal_projection","terminal_projection_sha256"},w);
    const auto path=text(row.get("path"),w,512); const Pin* selected=nullptr;
    for (const auto& pin:pins) { w.spend(8); if (pin.path==path) selected=&pin; }
    require(selected!=nullptr, "source: Mindmap10 closed original path");
    const auto global=integer(row.get("manifest_global_ordinal"),w);
    require(global==selected->global, "source: Mindmap10 manifest global ordinal");
    const auto fixtures=entireManifest.get("fixtures");
    require(fixtures.is_arr() && fixtures.size()==2145 && integer(entireManifest.get("schema"),w)==1, "protocol: Mindmap10 complete original manifest");
    w.spend(static_cast<std::size_t>(global)+1);
    const auto manifest=fixtures.at(static_cast<std::size_t>(global));
    require(text(manifest.get("path"),w,512)==path, "source: Mindmap10 manifest path at exact global ordinal");
    require(source.size()<=65536 && integer(row.get("source_bytes"),w)==static_cast<int>(source.size()) && integer(manifest.get("bytes"),w)==static_cast<int>(source.size()), "source: Mindmap10 complete byte count");
    const auto sha=hash(source,w);
    require(text(row.get("source_sha256"),w,64)==sha && text(manifest.get("sha256"),w,64)==sha, "source: Mindmap10 raw SHA-256");
    require(text(row.get("source_git_blob"),w,40)==text(manifest.get("git_blob"),w,40), "source: Mindmap10 Git blob identity");
    require(hash(canonical(manifest,w),w)==selected->manifest_sha, "source: Mindmap10 exact frozen manifest record/Git blob");
    const auto cases=doc.get("cases");
    require(cases.is_arr() && cases.size()==10, "protocol: Mindmap10 closed ten cases");
    pay(w,8192,1); std::string lock; std::size_t total_nodes=0,total_edges=0,total_mutations=0;
    for (std::size_t i=0;i<pins.size();++i) {
        w.spend(i+64); const auto r=cases.at(i);
        require(text(r.get("path"),w,512)==pins[i].path && integer(r.get("manifest_global_ordinal"),w)==pins[i].global, "source-lock: Mindmap10 sorted closed case identities");
        const auto raw=text(r.get("source_sha256"),w,64);
        require(raw.size()==64, "source-lock: Mindmap10 ASCII hex length");
        for (char c:raw) { w.spend(1); require((c>='0'&&c<='9')||(c>='a'&&c<='f'), "source-lock: Mindmap10 lowercase ASCII hex"); }
        append(lock,pins[i].path.substr(8),w); append(lock,std::string_view("\0",1),w); append(lock,raw,w); append(lock,"\n",w);
        const auto nodes=r.get("complete_model").get("nodes"), plans=r.get("required_good_first_mutations");
        require(nodes.is_arr() && nodes.size()>0 && nodes.size()<=64 && plans.is_arr() && plans.size()<=7, "protocol: Mindmap10 finite case counts");
        total_nodes+=nodes.size(); total_edges+=nodes.size()-1; total_mutations+=plans.size();
    }
    require(total_nodes==45 && total_edges==35 && total_mutations==60, "protocol: Mindmap10 complete responsibility totals");
    require(text(doc.get("source_lock_sha256"),w,64)=="cecb9535be0ad801effe77c4c76c653a0c1db3c6a6cb21f378a80f957c3320af" && hash(lock,w)==doc.get("source_lock_sha256").as_str(), "source-lock: Mindmap10 path/NUL/ASCII-hex/LF digest");
    const auto meaning=row.get("complete_model"), projection=row.get("terminal_projection");
    keys(meaning,{"nodes"},w); keys(projection,{"adaptation","nodes"},w);
    require(text(projection.get("adaptation"),w)=="loom_mindmap_terminal_projection_proposed/v1", "protocol: Mindmap10 projection version");
    require(hash(canonical(meaning,w),w)==text(row.get("complete_model_sha256"),w,64), "model-hash: Mindmap10 complete canonical Model");
    require(hash(canonical(projection,w),w)==text(row.get("terminal_projection_sha256"),w,64), "projection-hash: Mindmap10 complete canonical projection");
    const auto nodes=meaning.get("nodes"), rows=projection.get("nodes"), witnesses=row.get("source_witnesses");
    require(nodes.is_arr() && nodes.size()>0 && nodes.size()<=64 && rows.is_arr() && rows.size()==nodes.size() && witnesses.is_arr() && witnesses.size()==nodes.size(), "protocol: Mindmap10 complete node/witness/projection cardinality");
    pay(w,nodes.size(),sizeof(Node)+4096); Model m; m.nodes.reserve(nodes.size());
    std::size_t previous_end=0;
    for (std::size_t i=0;i<nodes.size();++i) {
        w.spend(i*3+128); const auto n=nodes.at(i), witness=witnesses.at(i), p=rows.at(i);
        keys(n,{"ordinal","caption","shape","parent","authored_id"},w);
        keys(witness,{"ordinal","caption","shape","parent","source_line","source_byte_start","source_byte_end","source_lexeme","authored_id"},w);
        keys(p,{"node_ordinal","caption_rows","row_joiners"},w);
        Node out; out.ordinal=integer(n.get("ordinal"),w); out.parent=integer(n.get("parent"),w);
        require(out.ordinal==static_cast<int>(i) && integer(p.get("node_ordinal"),w)==out.ordinal, "identity: Mindmap10 source/projection ordinal");
        for (const auto field:{"ordinal","caption","shape","parent","authored_id"})
            require(canonical(n.get(field),w)==canonical(witness.get(field),w), "source-witness: Mindmap10 exact ordinal/caption/shape/parent/authored_id five-tuple");
        out.caption=text(n.get("caption"),w,1024); out.shape=text(n.get("shape"),w,16);
        const auto id=n.get("authored_id"); require(id.is_null() || id.is_str(), "protocol: Mindmap10 authored ID type");
        if (id.is_str()) out.authored_id=std::string(text(id,w,1024));
        const int begin=integer(witness.get("source_byte_start"),w), end=integer(witness.get("source_byte_end"),w), line=integer(witness.get("source_line"),w);
        require(begin>=0 && end>begin && static_cast<std::size_t>(end)<=source.size() && static_cast<std::size_t>(begin)>=previous_end, "source-witness: Mindmap10 ordered byte span");
        pay(w,source.size(),2,256);
        require(line>0 && 1+std::ranges::count(source.substr(0,static_cast<std::size_t>(begin)),'\n')==line, "source-witness: Mindmap10 one-based source line");
        const auto lexeme=text(witness.get("source_lexeme"),w,2048);
        require(source.substr(static_cast<std::size_t>(begin),static_cast<std::size_t>(end-begin))==lexeme, "source-witness: Mindmap10 exact original byte lexeme");
        pay(w,out.caption.size(),4,1024);
        require(lexeme==(out.shape=="square" ? out.authored_id.value_or("")+"["+out.caption+"]" : out.caption), "source-witness: Mindmap10 complete authored lexeme");
        previous_end=static_cast<std::size_t>(end);
        out.rows=strings(p.get("caption_rows"),w); out.joiners=strings(p.get("row_joiners"),w); m.nodes.push_back(std::move(out));
    }
    require(validate_model(m,w).empty(), "model: Mindmap10 independently valid complete Model/projection");
    // These final closed pins also freeze mutation plans and historical metadata.
    // The consumer separately verifies the raw asset against kFactsSha.
    require(hash(canonical(row,w),w)==selected->row_sha, "fact-lock: Mindmap10 exact frozen whole row");
    require(hash(canonical(doc,w),w)=="112b25d22c69618dbb7d504b1b672ea65bec26029e144afbce6463791446ecc4", "fact-lock: Mindmap10 exact reviewed semantic document");
    return m;
}

namespace detail {
inline bool blank(std::string_view s) { return s.empty() || s==" "; }
// Orthogonal incidence bits: up, right, down, left.
inline unsigned bits(std::string_view s) {
    if (s=="│") return 5; if (s=="─") return 10;
    if (s=="└") return 3; if (s=="├") return 7;
    if (s=="┌") return 6; if (s=="┐") return 12;
    if (s=="┘") return 9; if (s=="┤") return 13;
    if (s=="┬") return 14; if (s=="┴") return 11;
    if (s=="┼") return 15; return 0;
}
struct Point { int x,y; };
struct Scene {
    int stop=0;
    std::vector<Point> markers, captions, outlines, branches;
};
inline Scene inspect(const Grid& g, const Model& m, std::string_view sentinel, Work& w) {
    w.spend(4096);
    require(g.width>0 && g.width<=240 && g.height>0 && g.height<=1040, "geometry: Mindmap10 Grid dimensions");
    const auto area=static_cast<std::size_t>(g.width)*static_cast<std::size_t>(g.height);
    require(g.cells.size()==area, "geometry: Mindmap10 complete Grid cells");
    require(!sentinel.empty() && sentinel.size()<=1024, "geometry: Mindmap10 following sentinel domain");
    const auto following=slots(sentinel,w);
    require(!following.empty() && following.size()<=static_cast<std::size_t>(g.width), "geometry: Mindmap10 following sentinel width");
    pay(w,area,4,1024);
    w.spend((64*2+256+1)*sizeof(Point)+2048);
    Scene scene;
    scene.markers.reserve(64); scene.captions.reserve(256);
    scene.outlines.reserve(1); scene.branches.reserve(64);
    int sentinel_count=0;
    for (int y=0;y<g.height;++y) for (int x=0;x+static_cast<int>(following.size())<=g.width;++x) {
        w.spend(1); if (g.at(x,y)!=following[0]) continue;
        bool same=true;
        for (std::size_t j=0;j<following.size();++j) { w.spend(1); if(g.at(x+static_cast<int>(j),y)!=following[j]) { same=false; break; } }
        if (same) {
            ++sentinel_count; scene.stop=y;
            for (int a=0;a<g.width;++a) { w.spend(1); require((a>=x && a<x+static_cast<int>(following.size())) || blank(g.at(a,y)), "geometry: Mindmap10 sentinel row contains foreign ink"); }
        }
    }
    require(sentinel_count==1, "geometry: Mindmap10 unique following sentinel");
    const auto active=static_cast<std::size_t>(scene.stop)*static_cast<std::size_t>(g.width);
    pay(w,active,sizeof(unsigned)+sizeof(std::string_view)+16,4096);
    std::vector<unsigned> routes(active,0);
    std::vector<bool> owned(active,false);
    auto offset=[&](int x,int y) {
        require(x>=0 && x<g.width && y>=0 && y<scene.stop, "geometry: Mindmap10 drawing crosses boundary/sentinel");
        return static_cast<std::size_t>(y)*static_cast<std::size_t>(g.width)+static_cast<std::size_t>(x);
    };
    auto claim=[&](int x,int y,std::string_view expected) {
        w.spend(expected.size()+8); const auto at=offset(x,y);
        require(!owned[at] && g.at(x,y)==expected, "geometry: Mindmap10 exact owned cell/padding"); owned[at]=true;
    };
    auto caption=[&](int x,int y,std::string_view row) {
        const auto glyphs=slots(row,w);
        for (std::size_t i=0;i<glyphs.size();++i) claim(x+static_cast<int>(i),y,glyphs[i]);
        scene.captions.push_back({x,y});
    };
    pay(w,m.nodes.size(),8*sizeof(Point)+512);
    if (m.nodes[0].shape=="square") {
        int left=-1,top=-1;
        for (int y=0;y<scene.stop;++y) for (int x=0;x<g.width;++x) {
            w.spend(1); if(g.at(x,y)=="┌") { require(left<0, "geometry: Mindmap10 unique square outline"); left=x; top=y; }
        }
        require(left>=0, "geometry: Mindmap10 missing square outline");
        const auto& n=m.nodes[0]; int max_width=0;
        for (const auto& row:n.rows) max_width=std::max(max_width,static_cast<int>(slots(row,w).size()));
        const int right=left+max_width+3, bottom=top+static_cast<int>(n.rows.size())+1;
        claim(left,top,"┌"); claim(right,top,"┐"); claim(left,bottom,"└"); claim(right,bottom,"┘");
        scene.outlines.push_back({left,top});
        for (int x=left+1;x<right;++x) { claim(x,top,"─"); claim(x,bottom,"─"); }
        for (std::size_t i=0;i<n.rows.size();++i) {
            const int y=top+1+static_cast<int>(i); claim(left,y,"│"); claim(right,y,"│");
            claim(left+1,y," "); caption(left+2,y,n.rows[i]);
            const int width=static_cast<int>(slots(n.rows[i],w).size());
            for (int x=left+2+width;x<right;++x) claim(x,y," ");
        }
    } else {
        // Recover the first physical marker on each occupied node row. Marker
        // glyphs within that row's verified caption belong to text, not nodes.
        for (int y=0;y<scene.stop;++y) for (int x=0;x<g.width;++x) {
            w.spend(1);
            if (g.at(x,y)=="●") {
                require(scene.markers.size()<m.nodes.size(), "geometry: Mindmap10 extra physical marker row");
                scene.markers.push_back({x,y}); break;
            }
        }
        require(scene.markers.size()==m.nodes.size(), "geometry: Mindmap10 physical node count");
        const auto origin=scene.markers[0];
        std::array<int,64> depths{};
        for (std::size_t i=0;i<m.nodes.size();++i) {
            const auto p=scene.markers[i]; const auto& n=m.nodes[i];
            require(p.y==origin.y+2*static_cast<int>(i), "geometry: Mindmap10 physical sibling/preorder row spacing");
            if (i) {
                const auto parent=index(m,n.parent,w);
                require(parent>=0 && parent<static_cast<int>(i), "geometry: Mindmap10 parent identity");
                depths[i]=depths[static_cast<std::size_t>(parent)]+1;
            }
            require(p.x==origin.x+4*depths[i], "geometry: Mindmap10 relative depth spacing");
            claim(p.x,p.y,"●"); claim(p.x+1,p.y," "); caption(p.x+2,p.y,n.caption);
        }
        auto route=[&](int x,int y,unsigned expected) { w.spend(8); routes[offset(x,y)] |= expected; };
        for (std::size_t i=1;i<scene.markers.size();++i) {
            const auto child=scene.markers[i]; int x=child.x-1; const int y=child.y;
            scene.branches.push_back({x,y});
            // Follow actual leftward horizontal ink until its upward turn.
            while (true) {
                w.spend(8); const unsigned b=bits(g.at(x,y));
                require(x>=0 && (b&2), "geometry: Mindmap10 broken child-left branch");
                if (b&1) { require(!(b&8), "geometry: Mindmap10 unexpected crossing/left route"); route(x,y,3); break; }
                require(b==10, "geometry: Mindmap10 non-horizontal child-left branch"); route(x,y,10); --x;
            }
            int py=y-1;
            while (py>=0 && g.at(x,py)!="●") {
                w.spend(8); const unsigned b=bits(g.at(x,py));
                require((b&5)==5, "geometry: Mindmap10 disconnected upward trunk"); route(x,py,5); --py;
            }
            require(py>=0, "geometry: Mindmap10 upward trunk has no parent marker");
            int physical_parent=-1;
            for (std::size_t j=0;j<i;++j) { w.spend(1); if(scene.markers[j].x==x && scene.markers[j].y==py) physical_parent=m.nodes[j].ordinal; }
            require(physical_parent==m.nodes[i].parent && child.x-x==4, "geometry: Mindmap10 first upward marker parent incidence");
        }
    }
    // Compare the complete union of observed routes, including every merged
    // incidence bit. All unclaimed ink before the following row is forbidden.
    for (int y=0;y<scene.stop;++y) for (int x=0;x<g.width;++x) {
        w.spend(4); const auto at=offset(x,y);
        if (routes[at]) require(!owned[at] && bits(g.at(x,y))==routes[at], "geometry: Mindmap10 exact route-bit union");
        else if (!owned[at]) require(blank(g.at(x,y)), "geometry: Mindmap10 unexplained ink/extra node/route");
    }
    return scene;
}
} // namespace detail

inline std::vector<std::string> validate(const Grid& g, const Model& m, std::string_view sentinel, Work& w) {
    const auto errors=validate_model(m,w); if(!errors.empty()) return errors;
    try { static_cast<void>(detail::inspect(g,m,sentinel,w)); return {}; }
    catch (const std::runtime_error& e) {
        if(std::string_view(e.what()).starts_with("resource:")) throw;
        w.spend(512); return {e.what()};
    }
}

namespace detail {
inline constexpr std::array<std::string_view,10> roster_hashes{{
"e8d43d6c0dd39afbd72ebd7ac51d9409b7c497ffd2700f23bb8251f99c7fd672",
"8f21fd1534d2389d3f031addd72829cbdb33cada55d0006725a76f1c63ff51e3",
"6bab3e9140cfaf79539672b6a11e46efcac60c5561786430548b34bfbc6e881d",
"195f52eb5430294a1789a1a3a72c19b9e61f930fa4b428364b60d9d58dc30894",
"63d8511a9321496a1c226ff6761ee6edb7114beec7d97a3432dff0be8acd1909",
"0727d525db613a39fdafaff519c5a8b5a64ff3ae711006a32df271a324a3ffd9",
"cf64205363fa6f112c1dde33067e0a033bebef877bd4737bdf2512925f0dccd4",
"035fd611b0424322ed1dbd5ff0b1cd62ef5482d35328894a1fb443a92568be92",
"0350dd10a5846346c9c91f7f1eb294d6f440c5cd8a8d6a8535faf1e503d74f40",
"d2e040956bf5643d0b52ed873be5f3870e9a44329a1bff09e3f0c09f5b9ab924",
}};
} // namespace detail

// roster is the selected frozen row's required_good_first_mutations array.
inline MutationSummary exercise(const Grid& g, const Model& m, JsonVal roster, std::string_view sentinel, Work& w) {
    using namespace detail;
    MutationSummary result;
    auto error=[&](std::string_view id,std::string_view why) {
        pay(w,id.size()+why.size(),4,256); result.errors.push_back(std::string(id)+": "+std::string(why));
    };
    require(roster.is_arr() && (roster.size()==5 || roster.size()==7), "protocol: Mindmap10 frozen mutation roster size");
    const auto sha=hash(canonical(roster,w),w);
    require(std::ranges::find(roster_hashes,sha)!=roster_hashes.end(), "fact-lock: Mindmap10 frozen per-original mutation roster");
    for (std::size_t r=0;r<roster.size();++r) {
        w.spend(r+256); const auto plan=roster.at(r); const auto id=text(plan.get("id"),w,128);
        // Repeat the same consumer-local good frame before EACH control.
        const auto good_model=validate_model(m,w);
        if (!good_model.empty()) { error(id,"good-first Model failed; no mutation credit"); continue; }
        Scene scene;
        try { scene=inspect(g,m,sentinel,w); }
        catch (const std::runtime_error& e) {
            if (std::string_view(e.what()).starts_with("resource:")) throw;
            error(id,"good-first frame failed; no mutation credit"); continue;
        }
        const bool semantic=id.starts_with("semantic_");
        bool rejected=false;
        if (semantic) {
            std::size_t bytes=0;
            for (const auto& n:m.nodes) {
                pay(w,n.caption.size(),1); bytes+=n.caption.size()+n.shape.size();
                for (const auto& row:n.rows) { pay(w,row.size(),1); bytes+=row.size(); }
                for (const auto& join:n.joiners) { pay(w,join.size(),1); bytes+=join.size(); }
            }
            pay(w,bytes,8, m.nodes.size()*(sizeof(Node)+1024)); Model changed=m;
            const auto node=plan.get("node_ordinal");
            const int at=node.valid()?index(changed,integer(node,w),w):-1;
            if (id=="semantic_caption_changed") {
                require(at>=0, "protocol: Mindmap10 caption mutation target");
                auto& n=changed.nodes[static_cast<std::size_t>(at)];
                // Replace one ASCII cell without invalidating the wrapping facts.
                std::size_t pos=0;
                while(pos<n.caption.size() && (static_cast<unsigned char>(n.caption[pos])>=128 || n.caption[pos]==' ')) { w.spend(1); ++pos; }
                require(pos<n.caption.size(), "protocol: Mindmap10 frozen caption has ASCII mutation cell");
                n.caption[pos]=n.caption[pos]=='Z'?'Y':'Z';
                if(n.shape=="square") { auto p=wrap(n.caption,w); n.rows=std::move(p.rows); n.joiners=std::move(p.joiners); }
                else { n.rows={n.caption}; n.joiners.clear(); }
            } else if (id=="semantic_last_leaf_removed") {
                require(at==static_cast<int>(changed.nodes.size())-1 && at>0, "protocol: Mindmap10 last leaf mutation target");
                changed.nodes.pop_back();
            } else if (id=="semantic_valid_parent_changed") {
                require(at>0, "protocol: Mindmap10 parent mutation target");
                auto& n=changed.nodes[static_cast<std::size_t>(at)]; const int parent=integer(plan.get("new_parent_ordinal"),w);
                require(n.parent!=parent, "protocol: Mindmap10 parent mutation must change incidence"); n.parent=parent;
            } else if (id=="semantic_valid_shape_changed") {
                require(at==0 && changed.nodes.size()==1, "protocol: Mindmap10 paired shape mutation target");
                auto& n=changed.nodes[0]; const auto shape=text(plan.get("replacement_shape"),w,16);
                require(shape!=n.shape, "protocol: Mindmap10 shape mutation must change shape"); n.shape=shape;
                const auto authored=plan.get("replacement_authored_id"); n.authored_id.reset();
                if (!authored.is_null()) n.authored_id=std::string(text(authored,w,1024));
                const auto p=plan.get("replacement_projection");
                require(integer(p.get("node_ordinal"),w)==n.ordinal, "protocol: Mindmap10 replacement projection identity");
                n.rows=strings(p.get("caption_rows"),w); n.joiners=strings(p.get("row_joiners"),w);
            } else if (id=="semantic_sibling_subtrees_swapped") {
                const auto children=plan.get("child_ordinals");
                require(children.is_arr() && children.size()==2, "protocol: Mindmap10 sibling mutation pair");
                const int first=integer(children.at(0),w), second=integer(children.at(1),w), parent=integer(plan.get("parent_ordinal"),w);
                const int a=index(changed,first,w), b=index(changed,second,w);
                require(a>0 && b>a && changed.nodes[static_cast<std::size_t>(a)].parent==parent && changed.nodes[static_cast<std::size_t>(b)].parent==parent, "protocol: Mindmap10 distinct ordered sibling subtrees");
                auto subtree_end=[&](int begin) {
                    int end=begin+1;
                    for (;end<static_cast<int>(changed.nodes.size());++end) {
                        int ancestor=changed.nodes[static_cast<std::size_t>(end)].parent; bool belongs=false;
                        while(ancestor>=0) {
                            w.spend(8); if(ancestor==changed.nodes[static_cast<std::size_t>(begin)].ordinal) { belongs=true; break; }
                            const auto j=index(changed,ancestor,w); require(j>=0, "protocol: Mindmap10 subtree ancestor"); ancestor=changed.nodes[static_cast<std::size_t>(j)].parent;
                        }
                        if(!belongs) break;
                    }
                    return end;
                };
                const int ae=subtree_end(a), be=subtree_end(b);
                require(ae<=b, "protocol: Mindmap10 disjoint sibling subtrees");
                // Adjacent rotations exchange whole subtrees and preserve IDs.
                std::rotate(changed.nodes.begin()+a,changed.nodes.begin()+b,changed.nodes.begin()+be);
                std::rotate(changed.nodes.begin()+a+(be-b),changed.nodes.begin()+ae+(be-b),changed.nodes.begin()+be);
            } else { error(id,"unimplemented semantic responsibility"); continue; }
            const auto valid=validate_model(changed,w);
            if(!valid.empty()) { error(id,"invalid semantic Model; no geometry credit"); continue; }
            const auto bad=validate(g,changed,sentinel,w);
            rejected=!bad.empty() && std::ranges::all_of(bad,[](const auto& s){return s.starts_with("geometry:");});
        } else {
            pay(w,g.cells.size(),sizeof(std::string)+8,sizeof(Grid));
            for (const auto& cell:g.cells) pay(w,cell.size(),2);
            Grid changed=g; Point p{-1,-1};
            const auto node=plan.get("node_ordinal");
            const int at=node.valid()?index(m,integer(node,w),w):-1;
            if (id=="physical_caption_cell_erased") {
                require(at>=0, "protocol: Mindmap10 physical caption target");
                p=scene.captions.at(static_cast<std::size_t>(at));
            } else if (id=="physical_marker_erased") {
                require(at>=0 && static_cast<std::size_t>(at)<scene.markers.size(), "protocol: Mindmap10 physical marker target");
                p=scene.markers[static_cast<std::size_t>(at)];
            } else if (id=="physical_outline_cell_erased") {
                require(at==0 && !scene.outlines.empty(), "protocol: Mindmap10 physical outline target"); p=scene.outlines[0];
            } else if (id=="physical_branch_cell_erased") {
                require(at>0 && static_cast<std::size_t>(at-1)<scene.branches.size(), "protocol: Mindmap10 physical branch target");
                p=scene.branches[static_cast<std::size_t>(at-1)];
            } else if (id=="physical_orphan_stroke_added") {
                // Use a blank cell after the final drawing row, never the sentinel.
                // If no padding row exists, any unoccupied pre-sentinel cell works.
                for (int y=scene.stop-1;y>=0 && p.x<0;--y) for (int x=g.width-1;x>=0;--x) {
                    w.spend(1);
                    if(g.at(x,y)==" ") { p={x,y}; break; }
                }
                if(p.x<0) { error(id,"no blank pre-sentinel mutation cell"); continue; }
            } else { error(id,"unimplemented physical responsibility"); continue; }
            require(p.x>=0 && p.y>=0 && p.y<scene.stop, "protocol: Mindmap10 physical mutation cell");
            const auto offset=static_cast<std::size_t>(p.y)*static_cast<std::size_t>(g.width)+static_cast<std::size_t>(p.x);
            w.spend(64); changed.cells[offset]=id=="physical_orphan_stroke_added"?"─":" ";
            const auto bad=validate(changed,m,sentinel,w);
            rejected=!bad.empty() && std::ranges::all_of(bad,[](const auto& s){return s.starts_with("geometry:");});
        }
        if(!rejected) error(id,"geometry accepted mutation; no rejection credit");
        else if(semantic) ++result.semantic;
        else ++result.physical;
    }
    return result;
}
} // namespace loom::test::mermaid_mindmap10

// RFC 0030 Shapes4. Independent, pure and deliberately separate from Original10.
// Consumers verify kFactsSha and take all five public captures BEFORE bind().
// No capture, file access, report writing or admission credit lives here.
namespace loom::test::mermaid_shapes4 {
using Work = mermaid_mindmap10::Work;
using Grid = mermaid_mindmap10::Grid;
using JsonVal = mermaid_mindmap10::JsonVal;
inline constexpr std::string_view kFactsSha = "26f803ffb4f900f81a5742f97671a7c6fd4a3c53a0d55a36a6bbbd2af5d1bd25";
inline constexpr std::string_view kFactsRelativePath = "mermaid_native/mindmap5_shapes_source_proposal.v1.json";
struct Node {
    int ordinal=-1, parent=-1;
    std::string caption, shape; // caption is the exact RAW semantic description.
    std::optional<std::string> authored_id;
    std::string display_caption, raw_caption_prefix, raw_caption_suffix;
    std::vector<std::string> rows, joiners;
};
struct Model { std::vector<Node> nodes; };
struct MutationSummary {
    std::size_t semantic=0, physical=0;
    std::vector<std::string> errors;
};
namespace detail {
using mermaid_mindmap10::canonical;
using mermaid_mindmap10::detail::slots;
using mermaid_mindmap10::detail::blank;
using mermaid_mindmap10::detail::bits;
inline void need(bool yes, std::string_view why) {
    if (!yes) throw std::runtime_error(std::string(why));
}
inline void pay(Work& w, std::size_t n, std::size_t scale, std::size_t fixed=0) {
    if (fixed>w.left || (scale && n>(w.left-fixed)/scale))
        throw std::runtime_error("resource: Shapes4 Work exhausted");
    w.spend(fixed+n*scale);
}
inline std::string_view text(JsonVal v, Work& w, std::size_t limit=65536) {
    w.spend(8); need(v.is_str() && v.as_str().size()<=limit,"schema: Shapes4 bounded string");
    pay(w,v.as_str().size(),1); return v.as_str();
}
inline int number(JsonVal v, Work& w) {
    w.spend(8); need(v.is_int() && v.as_int()>=-1 && v.as_int()<=65536,"schema: Shapes4 bounded integer");
    return static_cast<int>(v.as_int());
}
inline std::string digest(std::string_view s, Work& w) {
    pay(w,s.size(),8,256); return loom::utils::crypto::sha256(s);
}
inline void keys(JsonVal v, std::initializer_list<std::string_view> names, Work& w) {
    w.spend(128); need(v.is_obj() && v.size()==names.size(),"schema: Shapes4 exact key set");
    std::array<bool,32> seen{};
    need(names.size()<=seen.size(),"schema: Shapes4 key bound");
    v.iter_obj([&](JsonVal k,JsonVal) {
        w.spend(names.size()); const auto it=std::ranges::find(names,text(k,w,256));
        need(it!=names.end(),"schema: Shapes4 unknown key");
        const auto i=static_cast<std::size_t>(it-names.begin());
        need(!seen[i],"schema: Shapes4 duplicate key"); seen[i]=true;
    });
}
inline std::vector<std::string> strings(JsonVal a, Work& w) {
    need(a.is_arr() && a.size()<=256,"schema: Shapes4 bounded rows");
    pay(w,a.size(),sizeof(std::string)+64); std::vector<std::string> out; out.reserve(a.size());
    a.iter([&](JsonVal v) { const auto s=text(v,w,1024); pay(w,s.size(),2); out.emplace_back(s); }); return out;
}
inline std::pair<std::string_view,std::string_view> delimiters(std::string_view shape) {
    if(shape=="square") return {"[","]"};
    if(shape=="rounded") return {"(",")"};
    if(shape=="circle") return {"((","))"};
    return {"",""};
}
// Independent word/glyph projection; no production parser/layout is consulted.
struct Projection { std::vector<std::string> rows, joiners; };
inline Projection project(std::string_view caption, bool unwrapped, Work& w) {
    pay(w,caption.size(),16*sizeof(std::string)+128,1024);
    Projection out;
    if(unwrapped) { out.rows.emplace_back(caption); return out; }
    std::string row, pending;
    int cells=0;
    auto flush=[&](std::string_view next) {
        w.spend(row.size()+64);
        if(!out.rows.empty()) out.joiners.push_back(pending);
        out.rows.push_back(row); row.clear(); cells=0; pending=next;
    };
    for(std::size_t begin=0;begin<caption.size();) {
        pay(w,caption.size()-begin,1);
        const auto end=caption.find(' ',begin);
        const auto word=caption.substr(begin,end==std::string_view::npos?caption.size()-begin:end-begin);
        const auto glyphs=slots(word,w);
        if(!row.empty()) {
            if(cells+1+static_cast<int>(glyphs.size())>32) flush(" ");
            else { row+=' '; ++cells; }
        }
        for(std::size_t i=0;i<glyphs.size();) {
            w.spend(8); need(!glyphs[i].empty(),"model: Shapes4 detached continuation");
            const int measured=ftxui::string_width(glyphs[i]);
            need(measured==1 || measured==2,"model: Shapes4 whole glyph width");
            const auto width=static_cast<std::size_t>(measured);
            need(width<=glyphs.size()-i,"model: Shapes4 complete wide glyph slots");
            if(width==2) need(ftxui::string_width(glyphs[i+1])==0,"model: Shapes4 wide continuation width");
            if(cells+measured>32) flush("");
            row+=glyphs[i];
            if(width==2) row+=glyphs[i+1]; // A continuation can contain combining bytes.
            cells+=measured; i+=width;
        }
        if(end==std::string_view::npos) break;
        begin=end+1;
    }
    if(!row.empty()) flush("");
    return out;
}
inline void framing(const Node& n, Work& w) {
    need(n.raw_caption_prefix.size()<=1024 && n.raw_caption_suffix.size()<=1024,"raw-framing: Shapes4 framing byte bound");
    pay(w,n.caption.size()+n.display_caption.size()+n.raw_caption_prefix.size()+n.raw_caption_suffix.size(),4,256);
    need(n.caption==n.raw_caption_prefix+n.display_caption+n.raw_caption_suffix,"raw-framing: Shapes4 exact raw reconstruction");
    if(n.raw_caption_prefix.empty() && n.raw_caption_suffix.empty()) {
        need(n.caption.find_first_of("\r\n")==std::string::npos,"raw-framing: Shapes4 unframed single line"); return;
    }
    need(n.shape!="default","raw-framing: Shapes4 explicit multiline shape");
    const auto prefix=std::string_view(n.raw_caption_prefix), suffix=std::string_view(n.raw_caption_suffix);
    const auto newline=prefix.starts_with("\r\n")?std::string_view("\r\n"):std::string_view("\n");
    need(prefix.starts_with(newline) && suffix.starts_with(newline),"raw-framing: Shapes4 matching LF/CRLF");
    for(const auto slice:{prefix.substr(newline.size()),suffix.substr(newline.size())})
        for(char c:slice) { w.spend(1); need(c==' ',"raw-framing: Shapes4 exactly two LF/indent slices"); }
}
inline void check_model(const Model& m, Work& w) {
    w.spend(2048); need(!m.nodes.empty() && m.nodes.size()<=64,"model: Shapes4 node bound");
    std::array<int,16> ancestors{}; std::size_t depth=0;
    for(std::size_t i=0;i<m.nodes.size();++i) {
        const auto& n=m.nodes[i]; w.spend(128);
        need(n.ordinal==static_cast<int>(i),"model: Shapes4 contiguous source-order identities");
        need(i? n.parent>=0 && n.parent<static_cast<int>(i):n.parent==-1,"model: Shapes4 root/parent-before-child");
        if(i) { while(depth && ancestors[depth-1]!=n.parent) { w.spend(1); --depth; } need(depth>0,"model: Shapes4 open preorder ancestor"); }
        need(depth<16,"model: Shapes4 sixteen levels"); ancestors[depth++]=n.ordinal;
        need(n.shape=="default" || n.shape=="square" || n.shape=="rounded" || n.shape=="circle","model: Shapes4 closed non-Bang/Cloud shapes");
        need(!n.caption.empty() && n.caption.size()<=1024,"model: Shapes4 raw caption byte bound");
        need(!n.display_caption.empty() && n.display_caption.size()<=1024 && n.display_caption.front()!=' ' && n.display_caption.back()!=' ' && n.display_caption.find("  ")==std::string::npos,"model: Shapes4 plain caption spaces");
        for(unsigned char c:n.display_caption) {
            w.spend(1); need(c>=32 && c!=127 && std::string_view("[](){}<>`\"\\").find(static_cast<char>(c))==std::string_view::npos,"model: Shapes4 plain caption syntax");
        }
        const auto glyphs=slots(n.display_caption,w);
        need(!glyphs.empty() && glyphs.size()<=256,"model: Shapes4 caption cell bound");
        need(n.shape=="default"?!n.authored_id:n.authored_id.has_value(),"model: Shapes4 paired shape/authored ID");
        if(n.authored_id) {
            need(!n.authored_id->empty() && n.authored_id->size()<=1024,"model: Shapes4 ID length");
            for(std::size_t j=0;j<n.authored_id->size();++j) {
                const char c=(*n.authored_id)[j]; w.spend(1);
                need((c>='a'&&c<='z') || (c>='A'&&c<='Z') || c=='_' || (j && c>='0'&&c<='9'),"model: Shapes4 ASCII authored ID");
            }
        }
        framing(n,w);
        need(!n.rows.empty() && n.rows.size()<=256 && n.joiners.size()+1==n.rows.size(),"model: Shapes4 row cardinality");
        pay(w,n.display_caption.size(),4,1024); std::string restored;
        for(std::size_t j=0;j<n.rows.size();++j) {
            w.spend(32); need(!n.rows[j].empty() && n.rows[j].size()<=1024,"model: Shapes4 row bound");
            if(j) { need(n.joiners[j-1].empty() || n.joiners[j-1]==" ","model: Shapes4 joiner domain"); restored+=n.joiners[j-1]; }
            need(restored.size()+n.rows[j].size()<=1024,"model: Shapes4 reconstruction bound"); restored+=n.rows[j];
        }
        need(restored==n.display_caption,"projection-caption: Shapes4 row reconstruction");
        const auto expected=project(n.display_caption,n.shape=="default",w);
        need(n.rows==expected.rows && n.joiners==expected.joiners,"projection-caption: Shapes4 independent wrapping");
    }
}
} // namespace detail
inline std::vector<std::string> validate_model(const Model& m, Work& w) {
    try { detail::check_model(m,w); return {}; }
    catch(const std::runtime_error& e) {
        if(std::string_view(e.what()).starts_with("resource:")) throw;
        w.spend(512); return {e.what()};
    }
}
namespace detail {
struct Pin {
    int global; std::string_view path, row, manifest, raw, model, projection, witnesses;
};
inline constexpr std::array<Pin,4> pins{{
{1313,"mindmap/nightly-check/circle-shape.mmd","39cd3f40a831c2bcc5986bb73acb314161a6a6fdfd3e329a48d801faf179cdd2","d5d62ece27a5d14a71684a9d224c069b9e5968997bf2887f80ed715aaa513b37","691eee4a1c6791da16974a086ac7d34ee729ea9295330dbaa5dd79223fb8a37d","266ad653cf1c6f58881b8d6516711c728e13a416a6267635001f4edee2d0d3a6","3a708439facdade76c067311347c592bf664efd90cabe1f09586cf58e3fdc14a","3fb1b5dc96e95ab1c87cb40af5359eeb4bd3cecda1ab4c2f9405e4d7eacc00ee"},
{1317,"mindmap/nightly-check/rounded-rect-shape.mmd","b8fdda8b9ad53d942c7f5134ab4ab583fff918a1ebc4aa0e05c41be3eec1eed8","3869b837769abe8209d8cba798a22f3fa7dc484550d64992c0778fa5861769b2","332482160af16e6fbeaf15b7cf0bd39a2e53e85e3523c0a6d4a4820df1d23d72","84390d3ebae54375ecb99ed75aa991a892e69d6eea805e29808515684d788d65","3a708439facdade76c067311347c592bf664efd90cabe1f09586cf58e3fdc14a","5c92b2472cc9ea9184ccb6fa28811a60948eb3f665dacc91058cb3abeb6bd057"},
{1318,"mindmap/nightly-check/square-shape.mmd","a3bf0dd6b4dcdc7df8876d3b55970f754981090c845d62a4e7297b141fd8144d","34e6ec9a3ec639a52496192fbb4f942ccd07233bf11f8a3e081ac68890f9187b","907ac4e2fcf61b1322a5fe9fa0f7119f141ed8cf788e7942a2127be2051ddeb3","5dbb3df0453bf830d1d31c0a06a94e1772641031c69a670d6cef8e6ef2a62fb0","3a708439facdade76c067311347c592bf664efd90cabe1f09586cf58e3fdc14a","c9162d8099d04e8c0a12926d559ea617ca66642da0ddfb405988b50cfd931366"},
{1319,"mindmap/nightly-check/text-should-wrap-with-icon.mmd","590d2012752e55dfdbec8c3438bda4e7aa0a160d97e7626c8e70a90640253536","47161f9c82666537838bf2a1f4744efd9d392682ee465a0c724610a36fc5652d","3170766345ca188eeedc82ea4bcacbd4eb12d12f112fe38ed30b9b54c838cdb3","ab5ff831cbd6205e989bdafd4559f10d3593ff39dc196a8d9f387630837cbe8f","6153f0307b800bf2e914520e930a0f8d786a202c7b4f2a5c0a0629dbbbcbcd9e","3190873e1259ad3dab85df9f466bd72f773198b21f43b6842f0852f9473ca9c5"},
}};
} // namespace detail
inline Model bind(JsonVal doc, JsonVal row, std::string_view raw, JsonVal entiremanifest, Work& w) {
    using namespace detail;
    need(text(doc.get("schema"),w)=="loom_mindmap5_shapes_source_proposal/v1" &&
         text(doc.get("review_status"),w)=="source_design_reviewed_no_runtime_admission","schema: Shapes4 reviewed source document");
    need(number(doc.get("complete_originals"),w)==5 && number(doc.get("nodes"),w)==8 && number(doc.get("parent_relations"),w)==3 && number(doc.get("coverage_credit"),w)==0,"schema: Shapes4 source-only five-row totals");
    const auto review=doc.get("review");
    need(text(review.get("reviewer"),w)=="agent:Galileo" && text(review.get("verdict"),w)=="approved_source_and_bounded_shapes4_design_and_prr" &&
         text(review.get("reviewed_proposal_sha256"),w)=="b8f434d6dc927b05cdfb9cd4b0c28cb36a55c9fc13371d95615635e3d7996ca4","fact-review: Shapes4 approved proposal");
    const auto subset=doc.get("first_executable_subset_proposal"), ids=subset.get("cases");
    need(number(subset.get("complete_originals"),w)==4 && number(subset.get("nodes"),w)==5 && number(subset.get("parent_relations"),w)==1 && ids.is_arr() && ids.size()==4,"schema: Shapes4 four-source scope");
    for(std::size_t i=0;i<pins.size();++i) need(text(ids.at(i),w)==pins[i].path,"source: Shapes4 exact subset identities");
    const auto path=text(row.get("path"),w,512); const Pin* pin=nullptr;
    for(const auto& p:pins) { w.spend(8); if(p.path==path) pin=&p; }
    need(pin!=nullptr,"source: Shapes4 closed source identity; Bang/Cloud refused");
    const auto cases=doc.get("cases");
    need(cases.is_arr() && cases.size()==5,"schema: Shapes4 five-row source proposal");
    int matches=0;
    for(std::size_t i=0;i<cases.size();++i) {
        const auto r=cases.at(i); w.spend(i+8);
        if(text(r.get("path"),w,512)==path) {
            ++matches; need(canonical(r,w)==canonical(row,w),"source: Shapes4 row belongs to supplied document");
        }
    }
    need(matches==1,"source: Shapes4 unique document member");
    need(number(row.get("manifest_global_ordinal"),w)==pin->global,"source: Shapes4 exact global ordinal");
    const auto fixtures=entiremanifest.get("fixtures");
    need(number(entiremanifest.get("schema"),w)==1 && fixtures.is_arr() && fixtures.size()==2145,"schema: Shapes4 entire manifest required");
    w.spend(static_cast<std::size_t>(pin->global)+1);
    const auto record=fixtures.at(static_cast<std::size_t>(pin->global));
    need(text(record.get("path"),w)==path && digest(canonical(record,w),w)==pin->manifest &&
         digest(canonical(row.get("manifest_record"),w),w)==pin->manifest && text(row.get("manifest_record_sha256"),w)==pin->manifest,"source: Shapes4 manifest/Git record pin");
    need(raw.size()<=65536 && number(row.get("source_bytes"),w)==static_cast<int>(raw.size()) &&
         number(record.get("bytes"),w)==static_cast<int>(raw.size()),"source: Shapes4 complete raw byte count");
    need(digest(raw,w)==pin->raw && text(row.get("source_sha256"),w)==pin->raw && text(record.get("sha256"),w)==pin->raw &&
         text(row.get("source_git_blob"),w)==text(record.get("git_blob"),w),"source-hash: Shapes4 raw SHA/Git identity");
    need(text(row.get("complete_source_witness"),w)==raw && text(row.get("complete_source_witness_sha256"),w)==pin->raw,"source-witness: Shapes4 entire raw input");
    const auto meaning=row.get("complete_model"), projection=row.get("terminal_projection"), witnesses=row.get("source_witnesses");
    keys(meaning,{"nodes"},w);
    keys(projection,{"review_status","adaptation","geometry_contract","nodes"},w);
    need(text(projection.get("adaptation"),w)=="loom_mindmap_shapes4_projection_proposed/v1" &&
         text(projection.get("review_status"),w)=="source_design_reviewed_no_runtime_admission","schema: Shapes4 reviewed projection");
    const auto nodes=meaning.get("nodes"), projected=projection.get("nodes");
    const std::size_t count=pin->global==1319?2:1;
    need(nodes.is_arr() && nodes.size()==count && projected.is_arr() && projected.size()==count && witnesses.is_arr() && witnesses.size()==count,"schema: Shapes4 complete cardinalities");
    // Prepay all label/framing copies before constructing any Node or rows.
    pay(w,raw.size(),16*sizeof(std::string)+192,64*(sizeof(Node)+512));
    Model out; out.nodes.reserve(count); std::size_t previous=0;
    for(std::size_t i=0;i<count;++i) {
        w.spend(i*3+128); const auto n=nodes.at(i), p=projected.at(i), witness=witnesses.at(i);
        keys(n,{"ordinal","parent","caption","shape","authored_id"},w);
        keys(p,{"node_ordinal","display_caption","caption_rows","row_joiners","raw_caption_prefix","raw_caption_suffix"},w);
        for(const auto field:{"ordinal","parent","caption","shape","authored_id"})
            need(canonical(n.get(field),w)==canonical(witness.get(field),w),"model-source: Shapes4 witnessed semantic tuple");
        Node node; node.ordinal=number(n.get("ordinal"),w); node.parent=number(n.get("parent"),w);
        need(node.ordinal==static_cast<int>(i) && number(p.get("node_ordinal"),w)==node.ordinal,"identity: Shapes4 projection/source ordinal");
        node.caption=text(n.get("caption"),w,1024); node.shape=text(n.get("shape"),w,16);
        const auto id=n.get("authored_id"); need(id.is_null() || id.is_str(),"schema: Shapes4 authored ID type");
        if(id.is_str()) node.authored_id=std::string(text(id,w,1024));
        node.display_caption=text(p.get("display_caption"),w,1024);
        node.raw_caption_prefix=text(p.get("raw_caption_prefix"),w,1024); node.raw_caption_suffix=text(p.get("raw_caption_suffix"),w,1024);
        framing(node,w);
        const int begin=number(witness.get("source_byte_start"),w), end=number(witness.get("source_byte_end"),w);
        const int cb=number(witness.get("caption_byte_start"),w), ce=number(witness.get("caption_byte_end"),w);
        need(begin>=0 && static_cast<std::size_t>(begin)>=previous && end>begin && static_cast<std::size_t>(end)<=raw.size() && cb>=begin && ce>=cb && ce<=end,"source-witness: Shapes4 ordered byte spans");
        const auto lexeme=text(witness.get("source_lexeme"),w,4096);
        need(raw.substr(static_cast<std::size_t>(begin),static_cast<std::size_t>(end-begin))==lexeme &&
             raw.substr(static_cast<std::size_t>(cb),static_cast<std::size_t>(ce-cb))==node.caption,"source-witness: Shapes4 exact lexeme/caption spans");
        const auto [open,close]=delimiters(node.shape);
        need(text(witness.get("delimiter_open"),w)==open && text(witness.get("delimiter_close"),w)==close &&
             lexeme==node.authored_id.value_or("")+std::string(open)+node.caption+std::string(close),"raw-framing: Shapes4 full declaration delimiters");
        need(digest(lexeme,w)==text(witness.get("source_lexeme_sha256"),w),"source-witness-hash: Shapes4 lexeme");
        pay(w,raw.size(),2);
        need(number(witness.get("source_line"),w)==1+std::ranges::count(raw.substr(0,static_cast<std::size_t>(begin)),'\n') &&
             number(witness.get("source_line_end"),w)==1+std::ranges::count(raw.substr(0,static_cast<std::size_t>(end)),'\n'),"source-witness: Shapes4 physical lines");
        const auto linebegin=raw.rfind('\n',static_cast<std::size_t>(begin));
        const auto indent=static_cast<std::size_t>(begin)-(linebegin==std::string_view::npos?0:linebegin+1);
        need(number(witness.get("indent_bytes"),w)==static_cast<int>(indent),"source-witness: Shapes4 opening indent");
        for(std::size_t j=static_cast<std::size_t>(begin)-indent;j<static_cast<std::size_t>(begin);++j) { w.spend(1); need(raw[j]==' ',"source-witness: Shapes4 ASCII indent"); }
        previous=static_cast<std::size_t>(end);
        node.rows=strings(p.get("caption_rows"),w); node.joiners=strings(p.get("row_joiners"),w); out.nodes.push_back(std::move(node));
    }
    need(validate_model(out,w).empty(),"model: Shapes4 local Model/projection validity");
    need(digest(canonical(meaning,w),w)==pin->model && text(row.get("complete_model_sha256"),w)==pin->model,"model-hash: Shapes4 frozen raw Model");
    need(digest(canonical(projection,w),w)==pin->projection && text(row.get("terminal_projection_sha256"),w)==pin->projection,"projection-caption-hash: Shapes4 frozen display/framing projection");
    need(digest(canonical(witnesses,w),w)==pin->witnesses && text(row.get("source_witnesses_sha256"),w)==pin->witnesses,"source-witness-hash: Shapes4 frozen witnesses");
    need(digest(canonical(row,w),w)==pin->row,"fact-lock: Shapes4 whole selected row");
    return out;
}
namespace detail {
struct Point { int x=-1,y=-1; bool operator==(const Point&) const = default; };
struct Frame { int left=0,top=0,width=0,height=0; Point entry,bottom,caption,outline; };
struct Scene { int stop=0; std::vector<Frame> nodes; std::vector<Point> cuts; };
inline std::string_view contour(std::string_view shape, int x, int y, int width, int height) {
    const int r=width-1,b=height-1;
    if(shape=="circle") {
        if(y==0 || y==b || y==1 || y==b-1) {
            const int inset=(y==0 || y==b)?2:1;
            if(x==inset) return y<2?"╭":"╰";
            if(x==r-inset) return y<2?"╮":"╯";
            if(x>inset && x<r-inset) return "─";
        } else if(x==0 || x==r) return "│";
    } else {
        if(y==0) { if(x==0) return shape=="square"?"┌":"╭"; if(x==r) return shape=="square"?"┐":"╮"; return "─"; }
        if(y==b) { if(x==0) return shape=="square"?"└":"╰"; if(x==r) return shape=="square"?"┘":"╯"; return "─"; }
        if(x==0 || x==r) return "│";
    }
    return " ";
}
inline Scene inspect(const Grid& g, const Model& m, std::string_view sentinel, Work& w) {
    w.spend(4096);
    need(g.width>0 && g.width<=240 && g.height>0 && g.height<=1040,"geometry: Shapes4 Grid bounds");
    const auto area=static_cast<std::size_t>(g.width)*static_cast<std::size_t>(g.height);
    need(g.cells.size()==area,"geometry: Shapes4 complete Grid");
    need(!sentinel.empty() && sentinel.size()<=1024,"geometry: Shapes4 sentinel domain");
    const auto following=slots(sentinel,w);
    need(!following.empty() && following.size()<=static_cast<std::size_t>(g.width),"geometry: Shapes4 sentinel width");
    pay(w,area,8,4096); Scene scene; int found=0;
    for(int y=0;y<g.height;++y) for(int x=0;x+static_cast<int>(following.size())<=g.width;++x) {
        w.spend(1); if(g.at(x,y)!=following.front()) continue;
        bool same=true;
        for(std::size_t j=0;j<following.size();++j) { w.spend(1); if(g.at(x+static_cast<int>(j),y)!=following[j]) { same=false; break; } }
        if(same) {
            ++found; scene.stop=y;
            for(int a=0;a<g.width;++a) { w.spend(1); need((a>=x && a<x+static_cast<int>(following.size())) || blank(g.at(a,y)),"geometry: Shapes4 foreign sentinel-row ink"); }
        }
    }
    need(found==1,"geometry: Shapes4 unique following sentinel");
    const auto active=static_cast<std::size_t>(scene.stop)*static_cast<std::size_t>(g.width);
    pay(w,active,sizeof(unsigned)+sizeof(int)+8,64*(sizeof(Frame)+sizeof(Point)));
    std::vector<int> owned(active,-1); std::vector<unsigned> routes(active,0);
    scene.nodes.reserve(m.nodes.size()); scene.cuts.reserve(m.nodes.size());
    auto at=[&](int x,int y) {
        need(x>=0 && x<g.width && y>=0 && y<scene.stop,"geometry: Shapes4 drawing crosses bounds/sentinel");
        return static_cast<std::size_t>(y)*static_cast<std::size_t>(g.width)+static_cast<std::size_t>(x);
    };
    Point origin;
    for(int y=0;y<scene.stop && origin.y<0;++y) for(int x=0;x<g.width;++x) {
        w.spend(1); if(!blank(g.at(x,y))) { origin={x,y}; break; }
    }
    need(origin.y>=0,"geometry: Shapes4 missing physical root");
    if(m.nodes.front().shape=="circle") origin.x-=2;
    need(origin.x>=0,"geometry: Shapes4 root contour anchor");
    std::array<int,64> depths{}; std::array<bool,64> children{};
    int stride=0,top=origin.y;
    for(std::size_t i=0;i<m.nodes.size();++i) {
        const auto& n=m.nodes[i]; Frame f;
        int label=0;
        for(const auto& row:n.rows) label=std::max(label,static_cast<int>(slots(row,w).size()));
        f.width=n.shape=="default"?label+2:label+4;
        f.height=n.shape=="default"?1:static_cast<int>(n.rows.size())+2;
        if(n.shape=="circle") {
            f.width=std::max(9,label+6); f.width+=1-f.width%2;
            f.height=std::max(9,static_cast<int>(n.rows.size())+4); f.height+=1-f.height%2;
        }
        if(i) {
            const auto parent=static_cast<std::size_t>(n.parent);
            depths[i]=depths[parent]+1; children[parent]=true;
        }
        stride=std::max(stride,f.width+4); f.top=top; top+=f.height+2;
        scene.nodes.push_back(f);
    }
    for(std::size_t i=0;i<m.nodes.size();++i) {
        auto& f=scene.nodes[i]; const auto& n=m.nodes[i];
        f.left=origin.x+depths[i]*stride;
        need(f.left+f.width<=g.width && f.top+f.height<=scene.stop,"geometry: Shapes4 prescribed preorder intervals/depth columns");
        const bool shaped=n.shape!="default";
        f.entry={f.left,f.top+(shaped?f.height/2:0)};
        f.bottom={f.left+(shaped?f.width/2:0),f.top+f.height-1};
        f.caption={f.left+(n.shape=="circle"?3:2),f.top+(n.shape=="circle"?2:shaped?1:0)};
        f.outline={f.left+(n.shape=="circle"?2:0),f.top};
        // Claim every contour, caption, wide continuation and padding cell.
        for(int dy=0;dy<f.height;++dy) {
            const int row=dy-(n.shape=="circle"?2:shaped?1:0);
            const bool hasrow=row>=0 && row<static_cast<int>(n.rows.size());
            const auto glyphs=hasrow?slots(n.rows[static_cast<std::size_t>(row)],w):std::vector<std::string>{};
            const int start=n.shape=="circle"?3:2;
            for(int dx=0;dx<f.width;++dx) {
                w.spend(8); const int x=f.left+dx,y=f.top+dy; const Point p{x,y};
                std::string_view expected=shaped?contour(n.shape,dx,dy,f.width,f.height):dx==0?"●":" ";
                if(hasrow && dx>=start && dx<start+static_cast<int>(glyphs.size())) expected=glyphs[static_cast<std::size_t>(dx-start)];
                if(shaped && i && p==f.entry) expected="┤"; // UDL = 13, never URD = 7.
                if(shaped && children[i] && p==f.bottom) expected="┬"; // RDL = 14.
                const auto offset=at(x,y);
                if(owned[offset]>=0 || g.at(x,y)!=expected) {
                    w.spend(2048);
                    const auto actual=g.at(x,y);
                    throw std::runtime_error("geometry: Shapes4 complete physical shape/caption/padding/ports node="+std::to_string(i)+
                        " x="+std::to_string(x)+" y="+std::to_string(y)+" expected=<"+std::string(expected)+
                        "> actual=<"+std::string(actual.substr(0,64))+"> expected_bytes="+std::to_string(expected.size())+" actual_bytes="+std::to_string(actual.size()));
                }
                owned[offset]=static_cast<int>(i);
            }
        }
    }
    auto route=[&](int x,int y,unsigned mask) {
        w.spend(8); const auto offset=at(x,y);
        need(owned[offset]<0,"geometry: Shapes4 route crosses node interval"); routes[offset]|=mask;
    };
    for(std::size_t i=1;i<m.nodes.size();++i) {
        const auto entry=scene.nodes[i].entry; int x=entry.x-1; const int y=entry.y;
        scene.cuts.push_back({x,y});
        // Recover actual child-left branch until its upward turn.
        for(;;--x) {
            w.spend(8); need(x>=0,"geometry: Shapes4 missing child-left turn");
            const auto b=bits(g.at(x,y)); need((b&2)!=0,"geometry: Shapes4 severed child-left route");
            if(b&1) { need(!(b&8),"geometry: Shapes4 unrelated crossing at child turn"); route(x,y,3); break; }
            need(b==10,"geometry: Shapes4 horizontal child-left route"); route(x,y,10);
        }
        int py=y-1,physical=-1;
        for(;py>=0;--py) {
            w.spend(m.nodes.size()+8);
            for(std::size_t j=0;j<scene.nodes.size();++j) {
                if(scene.nodes[j].bottom==Point{x,py}) { physical=static_cast<int>(j); break; }
            }
            if(physical>=0) break; // First physical parent port, never skip one.
            need((bits(g.at(x,py))&5)==5,"geometry: Shapes4 disconnected upward trunk"); route(x,py,5);
        }
        need(physical==m.nodes[i].parent && py>=0 && py<y,"geometry: Shapes4 first parent incidence/source order");
    }
    for(int y=0;y<scene.stop;++y) for(int x=0;x<g.width;++x) {
        w.spend(4); const auto offset=at(x,y);
        if(routes[offset]) need(owned[offset]<0 && bits(g.at(x,y))==routes[offset],"geometry: Shapes4 exact reciprocal route-bit union");
        else if(owned[offset]<0) need(blank(g.at(x,y)),"geometry: Shapes4 unexplained ink/orphan/node/route");
    }
    return scene;
}
} // namespace detail
inline std::vector<std::string> validate(const Grid& g, const Model& m, std::string_view sentinel, Work& w) {
    const auto errors=validate_model(m,w); if(!errors.empty()) return errors;
    try { static_cast<void>(detail::inspect(g,m,sentinel,w)); return {}; }
    catch(const std::runtime_error& e) {
        if(std::string_view(e.what()).starts_with("resource:")) throw;
        w.spend(512); return {e.what()};
    }
}
// Fixed local controls, deliberately independent of the proposal's blocked plans.
// Only geometry rejection after BOTH good-first and mutant-local validity counts.
inline MutationSummary exercise(const Grid& g, const Model& m, std::string_view sentinel, Work& w) {
    using namespace detail;
    MutationSummary result;
    enum class Kind { Caption, Shape, Leaf, EraseCaption, EraseOutline, Orphan, Route, MissingChild };
    struct Plan { Kind kind; std::size_t node; };
    need(m.nodes.size()<=64,"model: Shapes4 mutation node bound");
    pay(w,64*8,sizeof(Plan)+64,4096);
    std::vector<Plan> plans; plans.reserve(m.nodes.size()*8+1);
    for(std::size_t i=0;i<m.nodes.size();++i) {
        plans.push_back({Kind::Caption,i}); plans.push_back({Kind::Shape,i});
        plans.push_back({Kind::EraseCaption,i}); plans.push_back({Kind::EraseOutline,i});
        if(i) { plans.push_back({Kind::Route,i}); plans.push_back({Kind::MissingChild,i}); }
    }
    plans.push_back({Kind::Orphan,0});
    if(m.nodes.size()>1) plans.push_back({Kind::Leaf,m.nodes.size()-1});
    auto failure=[&](const Plan& p,std::string_view reason) {
        pay(w,reason.size(),4,256);
        constexpr std::array<std::string_view,8> names{{"semantic_caption_changed","semantic_valid_shape_changed","semantic_last_leaf_removed","physical_caption_cell_erased","physical_outline_cell_erased","physical_orphan_stroke_added","physical_route_severed","physical_child_missing"}};
        result.errors.push_back(std::string(names[static_cast<std::size_t>(p.kind)])+" node "+std::to_string(p.node)+": "+std::string(reason));
    };
    for(const auto plan:plans) {
        const auto good=validate_model(m,w);
        if(!good.empty()) { failure(plan,"good-first Model invalid; zero credit"); continue; }
        Scene scene;
        try { scene=inspect(g,m,sentinel,w); }
        catch(const std::runtime_error& e) {
            if(std::string_view(e.what()).starts_with("resource:")) throw;
            failure(plan,"good-first physical frame invalid; zero credit"); continue;
        }
        const bool semantic=plan.kind==Kind::Caption || plan.kind==Kind::Shape || plan.kind==Kind::Leaf;
        std::vector<std::string> errors;
        if(semantic) {
            pay(w,m.nodes.size(),sizeof(Node)+4096);
            for(const auto& n:m.nodes) {
                pay(w,n.caption.size()+n.display_caption.size()+n.shape.size()+n.raw_caption_prefix.size()+n.raw_caption_suffix.size(),4);
                if(n.authored_id) pay(w,n.authored_id->size(),4);
                pay(w,n.rows.size()+n.joiners.size(),sizeof(std::string)+64);
                for(const auto& row:n.rows) pay(w,row.size(),4);
                for(const auto& join:n.joiners) pay(w,join.size(),4);
            }
            Model changed=m;
            if(plan.kind==Kind::Leaf) changed.nodes.pop_back();
            else {
                auto& n=changed.nodes[plan.node];
                if(plan.kind==Kind::Caption) {
                    // Replace the first whole glyph (including any combining bytes).
                    // Preserve width and stay valid at the 256-cell/1024-byte limits.
                    const auto glyphs=slots(n.display_caption,w);
                    const int width=ftxui::string_width(glyphs[0]);
                    need(width==1 || width==2,"model: Shapes4 caption mutant whole glyph width");
                    need(width==1 || glyphs.size()>1,"model: Shapes4 caption mutant continuation");
                    const auto bytes=glyphs[0].size()+(width==2?glyphs[1].size():0);
                    const auto replacement=width==2?"ZZ":glyphs[0]=="Z"?"Y":"Z";
                    n.display_caption.replace(0,bytes,replacement);
                } else {
                    n.shape=n.shape=="square"?"rounded":n.shape=="rounded"?"circle":"square";
                    if(!n.authored_id) n.authored_id="ShapesControl";
                }
                pay(w,n.raw_caption_prefix.size()+n.display_caption.size()+n.raw_caption_suffix.size(),4,1024);
                n.caption=n.raw_caption_prefix+n.display_caption+n.raw_caption_suffix;
                auto p=project(n.display_caption,n.shape=="default",w);
                n.rows=std::move(p.rows); n.joiners=std::move(p.joiners);
            }
            if(!validate_model(changed,w).empty()) { failure(plan,"mutant Model invalid; zero geometry credit"); continue; }
            errors=validate(g,changed,sentinel,w);
        } else {
            pay(w,g.cells.size(),sizeof(std::string)+16,sizeof(Grid));
            for(const auto& cell:g.cells) pay(w,cell.size(),2);
            Grid changed=g; const auto& frame=scene.nodes[plan.node]; Point p;
            if(plan.kind==Kind::EraseCaption) p=frame.caption;
            else if(plan.kind==Kind::EraseOutline) {
                // A port is contour ink too. The existing outline control cuts a
                // shaped parent bottom port or a shaped leaf child entry port;
                // standalone roots use a corner. No extra scoring plans.
                const bool parent=std::ranges::any_of(m.nodes,[&](const auto& n){ w.spend(1); return n.parent==static_cast<int>(plan.node); });
                p=m.nodes[plan.node].shape=="default"?frame.outline:parent?frame.bottom:plan.node?frame.entry:frame.outline;
            }
            else if(plan.kind==Kind::Route) p=scene.cuts[plan.node-1];
            else if(plan.kind==Kind::Orphan) {
                for(int y=scene.stop-1;y>=0 && p.x<0;--y) for(int x=g.width-1;x>=0;--x) {
                    w.spend(1); if(g.at(x,y)!=" ") continue;
                    const bool inside=std::ranges::any_of(scene.nodes,[&](const auto& box) {
                        w.spend(1); return x>=box.left && x<box.left+box.width && y>=box.top && y<box.top+box.height;
                    });
                    if(!inside) { p={x,y}; break; }
                }
                if(p.x<0) { failure(plan,"no blank orphan target; zero credit"); continue; }
            }
            auto erase=[&](int x,int y) {
                w.spend(16); need(x>=0 && x<g.width && y>=0 && y<scene.stop,"protocol: Shapes4 physical mutation bounds");
                changed.cells[static_cast<std::size_t>(y)*static_cast<std::size_t>(g.width)+static_cast<std::size_t>(x)]=" ";
            };
            if(plan.kind==Kind::MissingChild) {
                for(int y=frame.top;y<frame.top+frame.height;++y)
                    for(int x=frame.left;x<frame.left+frame.width;++x) erase(x,y);
                const auto parent=scene.nodes[static_cast<std::size_t>(m.nodes[plan.node].parent)].bottom;
                for(int x=parent.x;x<frame.entry.x;++x) erase(x,frame.entry.y);
                for(int y=parent.y+1;y<frame.entry.y;++y) erase(parent.x,y);
            } else {
                need(p.x>=0 && p.y>=0 && p.y<scene.stop,"protocol: Shapes4 meaningful physical target");
                if(plan.kind==Kind::Orphan) {
                    w.spend(16); changed.cells[static_cast<std::size_t>(p.y)*static_cast<std::size_t>(g.width)+static_cast<std::size_t>(p.x)]="─";
                } else erase(p.x,p.y);
            }
            errors=validate(changed,m,sentinel,w);
        }
        if(errors.empty() || !std::ranges::all_of(errors,[](const auto& s){ return s.starts_with("geometry:"); }))
            failure(plan,"no geometry-only rejection; zero credit");
        else if(semantic) ++result.semantic;
        else ++result.physical;
    }
    // No reversed-parent/sibling credit is invented for the two-node original.
    return result;
}
} // namespace loom::test::mermaid_shapes4
