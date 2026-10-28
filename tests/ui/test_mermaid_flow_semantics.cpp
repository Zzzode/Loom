// RFC 0030 Phase C2: an independent cell-geometry oracle for the frozen
// Beautiful Mermaid flowchart inputs. No upstream rendering code is used.
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>

import std;
import loom.ui.visual.markdown;
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_flow400_linkprofile3_authority.hpp"
#include "mermaid_flow_group_endpoint15_oracle.hpp"
#include "mermaid_flow_shape_alias_authority.hpp"
#include "mermaid_flow_title_authority.hpp"

namespace {
namespace json = loom::utils::json;
using json::JsonVal;
namespace flow_oracle = loom::test::mermaid_flow;
using namespace flow_oracle;

Grid mock(std::initializer_list<std::string_view> lines) {
    int width = 0;
    for (const auto line : lines)
        width = std::max(width, ftxui::string_width(std::string(line)));
    Grid result{width, static_cast<int>(lines.size()), {}};
    for (const auto line : lines) {
        // Utf8ToGlyphs already reserves a continuation entry for every wide
        // character. Each vector entry corresponds to exactly one screen cell.
        const auto end = result.cells.size() + static_cast<std::size_t>(width);
        auto cells = ftxui::Utf8ToGlyphs(std::string(line));
        result.cells.insert(result.cells.end(), cells.begin(), cells.end());
        while (result.cells.size() < end)
            result.cells.emplace_back(" ");
    }
    return result;
}

struct FrozenCase {
    const char* id;
    const char* input;
    const char* model;
    const char* assertions;
    const char* provenance;
};

constexpr std::array<FrozenCase, 66> kFrozen{{
    {"beautiful_mermaid_ca91f05e3b644cdc",
     "ca91f05e3b644cdc96baec40a5a9f943e32d3dd1a117d73f8603c3f5c7be16f1",
     "398ccc2b646bc3210a987a9499d601cbf1f8d3f3968e79c089039263dda15eff",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "dc1568a3e3e91162552ad054afa00d8174d4ca69d36a21c391a775fbf258d787"},
    {"beautiful_mermaid_28971171f9b73531",
     "28971171f9b7353191b200be357814a7a420b62f375051515d623d582133cbd5",
     "fbb413d75b8aa87e055b629bcebccf491aa019df3b1e3e03f2e7cba331c2e9d8",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "01e1131fa34882ea112c2a93ad2d67d9ce03dc0712bc7ee8d43e87089a74eafb"},
    {"beautiful_mermaid_8a8860655bacad58",
     "8a8860655bacad582c8a7dd0ea3679db88af232c23e972a1b5a9657e27eb7139",
     "20f37b92bf18381f403804917b79df96248773d9e030810ce663d6e9f587ced6",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "74766a9eb77cfb89f3a12f1b016d90b1e54f2ec17ab966b8e1912eb7acd85041"},
    {"beautiful_mermaid_4713b9809f5a63ae",
     "4713b9809f5a63ae111c980c56af6f289d484d342c8d451311bed05986fc9dbb",
     "89abc05c5c8078107d7a9ac916ce5956ce32e0035fe9d53d5d64c86d173a5fb1",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "9d11848a3de71fe97ee63a81f0d3987156cdb7a9dba9cef9ffeb109bf6627567"},
    {"beautiful_mermaid_dabeed9e1d12c861",
     "dabeed9e1d12c86195b2429fad8580ae1ff45947f8342d15340a4316fd10c378",
     "6012f6428ceda3abe63854d5c1ddc09752a2397015d4f921c402ccbe3d6e68de",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "6032379e0175b1ec78a03d16b84c7b5e283de4df6ddf3b50522ce6cdccd9fde7"},
    {"beautiful_mermaid_22775b7f44c796b6",
     "22775b7f44c796b6df5ce850c7cf7fbb0af771d895c9b0f3f66e8b30fdf62871",
     "2439ccef6a3d5059d9ad5609e53bc644bb8e645ca8ed68e2a21f0805d7d437a7",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "30f8e6c5bf9ed59d8f6e9bc9693511104012d59e15836b671c3f8748943b45a5"},
    {"beautiful_mermaid_ab3cc696ae6a1152",
     "ab3cc696ae6a11529514bd8bf8b83b644c1064239b507e84d4f42984fcd41e1a",
     "7fad753f053e704d4d04f7e8aa592fa4284e9588cb291ff0c5773ac9c0a05cf4",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "9683c51873db874695027a472687d4c53cd7ca34f880d6af5321d28652bdb7bf"},
    {"beautiful_mermaid_ec3ff130d8cf8d82",
     "ec3ff130d8cf8d82ab3b78d40a1faa21577e909f492b682ee9b5263539194feb",
     "f6ac51aad24453dba4345fd03f36e3b09991d2c64de661d0775770c76690e029",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "66cf76fcdd814f1fac8eb057f1d08659c17a100092e2eadd7351b413d5fc5f02"},
    {"beautiful_mermaid_bd2129e8c0523bc6",
     "bd2129e8c0523bc68f6bfed1399cc8cfb2bc940b062604c085e8676f66e9481c",
     "294d688a2f12a950be4d5e7d689fc481f6a074333e1e91c668fd0f1db76b45ad",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "fd5f26e57f89f5697d02899b83cdacfd02b3311f8db1d67abb438db35b0581a8"},
    {"beautiful_mermaid_7e39b1bb55c5437e",
     "7e39b1bb55c5437e43ea689a911499a6cbff7235dc2e0b844a9f597caf8a6df1",
     "100e2b597ac82250460bd2db4e6cc71b3c31fd0cbea9a090b4df56eb8a0b84c2",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "a1347574f50e7a1e2835bd916fad92e0233ca3e8b9ccedf1a0ed7e795e0a4eea"},
    {"beautiful_mermaid_f83a66b06293b6d3",
     "f83a66b06293b6d34c42b420db4504f24a4acc97f316a94c02669a3aa3fe30f6",
     "0a66d7f51b7d417d8a800f1a11fb690745660a9b3d7bc279149ca945e548cef4",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "22368129d441e3df728cdab950b3098ea293104f96b97665530a6e1d0d135355"},
    {"beautiful_mermaid_d5da5410935f96a1",
     "d5da5410935f96a1581fefdc3b3d2e2caf10dde2b742b97cf10c6680c1e991ac",
     "6aac16a407e3b1ef92a7c4e7f3414bb8fde8b751132aaff7c86381bb66c23c9d",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "39f42d9225fa50074348d3ed0d519cbeb4fbe8292fc5233ce5b5264687142464"},
    {"beautiful_mermaid_cbd2a27458b6a8a6",
     "cbd2a27458b6a8a6615b19132981fcc0a5043b8e8b77cff222e9b1c7636b5a7d",
     "e2db534cb8730736eb1a1f34886e40b76aecdc6aebd9031024841d7861a177e1",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "921ce0123b5759ce77cbdaa7268e44b5cd14960fd9e8afbaec93128b927c18af"},
    {"beautiful_mermaid_5d9c09d844a3e2ba",
     "5d9c09d844a3e2ba733ad5e3ad604e9ed6f11f84ab070c9bed83635dd485c761",
     "5d8d12966ea6a194914d6c40c0d3d9144685e5e8d00bccbab1b15b13d28006f5",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "00375c54a45b197b0f013e5e1200aa6fad244d8c85a92d3ced29a2ed3926c050"},
    {"beautiful_mermaid_cc4cab1a75a260a6",
     "cc4cab1a75a260a62a2b468959b25f669d55467460294fb76e3f71bf99dd1bf5",
     "ad57fbb3a65e5b14da820f4f227430c56b9c2de83284303121803b52d7bb191e",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "0daa9fec645ba610cf663b253102acbc098b367af9a76437a215cf1ae78417c0"},
    {"beautiful_mermaid_be3ce2196f32dd36",
     "be3ce2196f32dd36e8823008a5517a1f425474ac49db8761d4f5095ef0d904b2",
     "3d810149f0da4869443029a72bc63567bd2ca581a873d4275b08c9d3d6d21b5e",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "c088a61be03e81e7f3942329ece5c85a846e01d9f6b38dc752d6a5f5cb3280d9"},
    {"beautiful_mermaid_eead33eba3471656",
     "eead33eba347165699f77de19b7d5d44d69d77a5d4e5fac399b2f576e08df501",
     "4e4e38b4329a4cb9b64a51f4d8190198ede9be87a8ca0793856a3e7b37a694b2",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "c99a9e0c9b4eebd7b872581a0d4fb43ab5908e35142928ae8d368a7fb3fc7b3c"},
    {"beautiful_mermaid_652f3f1d1a9f254b",
     "652f3f1d1a9f254b908b335093c87f1aef7fa396b8e953c0f0ff252949b3e884",
     "9cd697ba2395971dfae2a8013533c5460d75ec3435ba3a9a8c91d94ea8051f06",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "3b57a90a2610144628794643409c4567d74577037bda20a34f67382d74a3dd2c"},
    {"beautiful_mermaid_fa4f27cf423eb0f3",
     "fa4f27cf423eb0f39edf31d85e536b36d9537ec0e2ea9e8d66b52f37f0f542eb",
     "722998ee8721f70db2a3c6cd74c241ace0bd76f89f965532971ecf157c3fda56",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "885c6c5467339e217c9832c2dacf177a6e5cf4a9ddfaaf0dfc9133d5168fa398"},
    {"beautiful_mermaid_4d63acfbcbd54d4f",
     "4d63acfbcbd54d4f64c4b6d78ceee818433a949e7b1ce526fb8ee1b4529ae4d5",
     "194eb8147121339ee97c6f7354d77d4f9c1e04c22c453c14f7f2e5c3de9930b3",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "09ec8ef553a65d386c4100d7018541c168fb30b3fbb409599e1b83188643811a"},
    {"beautiful_mermaid_a184bb4eeb3a8b1b",
     "a184bb4eeb3a8b1bd0f32700b1131ccad139c955e68a88ac5f383e6006282efc",
     "21526794aab3fac807266279da6d749f690d0a12a9ace05272b23cd0bd164685",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "f296b60963eface3b595873c7c413fd3fc57f5c1ac5a572d23763ccb32c2b2be"},
    {"beautiful_mermaid_1356da98c70cdb34",
     "1356da98c70cdb34b3d49b289526c931f9e5d2cdae62405150b199562d9a66b5",
     "b1ff428e6c3fc3c323902408bfb9fb3493e240e525cfec38a843d0893a1d4cc2",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "a97e52d3af416be1fbee6d3bf4c13690f12b460c44da20b7052e9882fc6310b1"},
    {"beautiful_mermaid_eff4314936781a0b",
     "eff4314936781a0b4c097aa9c369dd41bfbb3b2ca1490ebb9a5fec2008a503e5",
     "a422dc2907365427daae02cdedb602a4d81fe2f6d2648e1703c405dd30fe2f46",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "3c1b41e008c944b88b7f9eba8a9f7791b71e66123ad41a67346bcd2b5e982526"},
    {"beautiful_mermaid_89c98aec3629b5b5",
     "89c98aec3629b5b5caf5e43a13a33e6f0578cc4299245f2c36b8d461ea32a079",
     "76c0718d3521e1521d05d45def8666ffdeca68981cb11c145126fe48bd1875ee",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "0300bf155f7653c4425a33a928726f4051407014c288c59274c7d4e730f936ab"},
    {"beautiful_mermaid_779b8bbb6ebe595a",
     "779b8bbb6ebe595af1475f813f72d6a533445060ed9d6ad54b124cea6b7fd735",
     "91f089682552b0026da00f297e864d3e49323c254f87aab1347b45fd168db81f",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "a1ded1850f8b7d2fa88b43f9f0a3abe8bce1e8753387dd0bc5c1fac080bce0d8"},
    {"beautiful_mermaid_f4ab2c8363c33100",
     "f4ab2c8363c331005aed4c6a9b0c90e2da9aeb2b78794f37b3f1458de5915bc9",
     "10103c2ccb13393d0499ca0abab0da0233481202ec930b9a273057ec90d52959",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "29438a1b5ef404cb81cfcfd558251b1b96799fabcddb27fa1e711714a31b2d1c"},
    {"beautiful_mermaid_db4ae15e7a656a90",
     "db4ae15e7a656a90803657a410ceb7e2c99ef99794aab877e0b40bfb647b3641",
     "64dfb9807e96a9c3ad461226acaa7ce389547e3c76937b2858bdba0ec8e4fca1",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "a1897659bab8543206c6e0b48c2dddab8d8e4734ea967a7414a9aab913e40b5c"},
    {"beautiful_mermaid_fab39a2080922ce5",
     "fab39a2080922ce5a66f1167993e95a0c24d4ae1170bcdea9e8b253d7484fdf9",
     "f3eabdc4bfba496a58559341109c6b2ba279c7d7f5d0a3c63a511d578a07996e",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "08a4c6ad0ccab64884dc9d08e9f5a79cb59086aced05ecaa76aa3550d1a1e9d9"},
    {"beautiful_mermaid_6a0f5bd646322b0c",
     "6a0f5bd646322b0cbbcff7c6435206df4225f60baae807c336d36b1a4ad2af1d",
     "adf2b00742948abab79240f90f5576a0723dac327ae7d139dfe6e4d7b4acdcf8",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "f1337b4de76c08dbe0a83d747458290c945ff61d42c0e0885022424c5efe93e8"},
    {"beautiful_mermaid_a90544cac88395d9",
     "a90544cac88395d91052d30d6af0f4f12341aaf24431d95735c31ecfbdee97af",
     "6e6b1f3eba83c28af3cb8aaff476b9274dc0934040479554439d42ced743a07a",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "6bfa7b380f5cbb812bdbd4a18c72bf9df5b78e85deee374b7937b7a111ad1f0f"},
    {"beautiful_mermaid_e2391f288e211af3",
     "e2391f288e211af3717404051908c7c85ceea806ce1ba1ecaab06f31e2320a42",
     "9e6053636cf0bd1b60f71e3515f6274e52409db6a41a30b08c35781e59c1b6a8",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "7964648ef504bf051712458873b6f104945c51d13628d2c6d0fb36281a61b150"},
    {"beautiful_mermaid_64a8caeadbe77777",
     "64a8caeadbe7777740798a7e20e495b75aed5e36d4d60e1a583607d8cfbbb282",
     "d8915f60f6899d39c3795e0f4532d79c6cd564728f0c1cf3c68adba54c34168d",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "08ec5896d78d35509fdef677e7a0328e09db09ef5c60a31144d7e61471811147"},
    {"beautiful_mermaid_62fed1969c0d94f2",
     "62fed1969c0d94f26f71fb3b49170735aa8353c4905f27bd1eb375c222d6a36e",
     "4d62f79145383ac0b8f91d3967d70361d9c6af581f007dd3b8a19bbc5009d3a1",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "6d4249ea1a1c95ba78da66a27fc7542b1a9f9f3b611723515780ef3ef4f840e3"},
    {"beautiful_mermaid_960bc64ea24e4e72",
     "960bc64ea24e4e728d6583ff40ef8994ea3ac2a99d719b2782ddbf7d1dda9efd",
     "3164e785bb57a7e2e4df1cbfaa1f2d68fa67374581a83280ee6994618457230d",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "f149b3078bdaff94ef0dd514d042a140aacb71df8330d6b6073b36c5fe555318"},
    {"beautiful_mermaid_7c85a895c03a56dc",
     "7c85a895c03a56dc18b6c96a736fd82946a56338b614c58dd3027097d2d5c921",
     "323745884deb1b309a8ff205ffbe9c14c9248198684cf3643eba580f896382c9",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "198b98bde612d78804751d7bca85623af2acc97b8e6a6c10e28e87a37fa7873f"},
    {"beautiful_mermaid_694971b8aa1b7980",
     "694971b8aa1b7980d65808daea60b7475ab1c7bddac6a6305f33cf38d6fb95bd",
     "cc3ab3cf7695ae6708266a27bd9a1f82bee6dc50b4a568fe25a2e9d8ac7d3453",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "fd28af8491f30e9ec033f497543d997f67b91352cf6d4b9a09a93ee586db054b"},
    {"beautiful_mermaid_75ff3f3be94560a4",
     "75ff3f3be94560a47aee84098504ea8e4910ddcadc78c324394b9fcd56152ce0",
     "4454a6b10b4163e50078cf452333868f824bfabc9d030a04753f7f607078c1fe",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "29e5aaf6b290137d488036cc7875c7b9259f4a8220c8a581037049ef470b4c5e"},
    {"beautiful_mermaid_49efeb61f4d84004",
     "49efeb61f4d8400488240106b192831e09baee79db1aa2305626ff87c5c4bfce",
     "82f1df54a827d2146488696817f0df2a6a1bf5016fc781ec1327197c268be083",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "f2c4a186e2638f81a432e1fec02e88d73eb84a23964c8bd765310ecf2aab7149"},
    {"beautiful_mermaid_4817391338628a4f",
     "4817391338628a4f0e6a3fd86174718df7f493a470c79280e3b97662c6832953",
     "49067b9a010cc4852a4f9c014c03feb1b26887dc82b1f45c1b72222a7a2b577c",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "4cb096890c5c95765e78a922b204c865f4fc911c4feb0fd1c744a3df3d48d6da"},
    {"beautiful_mermaid_62cf7fde3fead157",
     "62cf7fde3fead157df30f3249775402e4233c68eb6f78b7b17cce3ade707cddb",
     "28ecea0aeed1e791387fc8235af1fde60338280bcbeb1e10631dc5136c1d2b91",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "bd4bfa29782de3fc5ef94e6d6e84c0b746edc693f6f1af4a9c386b38b3cae593"},
    {"beautiful_mermaid_bfabcd2f221e905f",
     "bfabcd2f221e905f451a9254ccbfeace98ced0475e61bd5d433382335b9d7d96",
     "673a19986d4b32f734d373061a5db22051c62fad61167f4c86679022721889ba",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "65918803523ad309a7b59c2e0e658736fe8c95837601788ee2fd6d82270caa0c"},
    {"beautiful_mermaid_09359d685333c23b",
     "09359d685333c23b5e093507aa92687712a8a8ad7ee8f3fc5e086b47d415e1c2",
     "673a19986d4b32f734d373061a5db22051c62fad61167f4c86679022721889ba",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "159e4b04c6b86f5a89ccdf0b66125fc172d84e7048785365cec28efc84c70f13"},
    {"beautiful_mermaid_0fe9361e84b2de2f",
     "0fe9361e84b2de2f6dc99d7bf7dd322b7e2b82ff2c855f64f963991f2ff4dbda",
     "398ccc2b646bc3210a987a9499d601cbf1f8d3f3968e79c089039263dda15eff",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "1258e4c2141d254aae8945f81849d8383e7a2a1dc5a2de27c1e34b7dd566feaa"},
    {"beautiful_mermaid_b9a470bd51cedad3",
     "b9a470bd51cedad38f9e864b369b0012c9900af77a8efac16d196aba9dc34a7c",
     "37e862dc11c4eea435edd7d4f82c08e9f62a41b4f404ad342e6b2e6c4d95185a",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "c8800e683304d9c4f9966b40041730b606df39c15c8b1813baef9702a9244644"},
    {"beautiful_mermaid_8e4eb638edc21f05",
     "8e4eb638edc21f059880130cede83b36e54034d2e26b88891249803ae6da10d3",
     "afae4fdb3e5bb0b80a0f7fac523d5c72522855973b616d680dbb795ae96f5c03",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "b58feebdb3c70ac9f4dba399de7e405e0f13ae21466250bdc100ab66b321386f"},
    {"beautiful_mermaid_93498fb81f634350",
     "93498fb81f6343504d10f4306dfc4675a3583a123da4981f15efdfc1aaeeb2b9",
     "12184b953cf1cd65d044762d80bca9eadce5b2695952c98a3d5ba8674a5d53d5",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "fd827d34c06b488ab7bc5ff17cdee39ed7142a87ccf9b3707f1f6bef8fdfee6a"},
    {"beautiful_mermaid_d52b9ccd6f7e09d2",
     "d52b9ccd6f7e09d243b91af99926cffe4c8aa072325ba92633403f1e26992168",
     "4e0e0a2eb481a89167071015d4be25c29585d56d3e275317bbe0992a382e37e8",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "2d6bf656e2f4b378f4fdfd2e10e5f3f62d1e06e94c62672c944340bf52dc263b"},
    {"beautiful_mermaid_804cf6e756c0cc7c",
     "804cf6e756c0cc7c9a9b8387fef66bb2d6a6d4910a5f4cf2f3623a89b89dffb4",
     "2439ccef6a3d5059d9ad5609e53bc644bb8e645ca8ed68e2a21f0805d7d437a7",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "e4bd50ca8e171e97f7dc80aa51259daab47abf7d383f9eddfd995f4e9214c2c2"},
    {"beautiful_mermaid_73c48347987b829a",
     "73c48347987b829af1481d7910dea7f8083aa3b996a689285fcc85d27fdf2068",
     "6aac16a407e3b1ef92a7c4e7f3414bb8fde8b751132aaff7c86381bb66c23c9d",
     "5a0396049d04a6b27ef1e26bcefaf8918ce7a6c6e452c5685e38c116f1a4a3b8",
     "8a6f25e7c3ba5edbf318126a30c44722dc1ee33a1285f6009ce0e572c1453095"},
    {"beautiful_mermaid_b37bebe1a745f1d6",
     "b37bebe1a745f1d6f511b8ae13ec71a3bddbbd567e5193705cd5e513204e966a",
     "6b0f00ad859cee53beb7139ca43951ef4af98b3a7f3cd8115ee47810e0b07405",
     "3c21a07c5e66a5e1413cad47c3709cfa560146056fcc06bbc5de4141e8cf4d5c",
     "aa7741e35fb8a9cce6459f9d15604db053b0c4e02b1a1e3012ff8e9ddd390b1d"},
    {"beautiful_mermaid_21dee2b309ed3823",
     "21dee2b309ed38234484973d987acae9ea87056dcf2ebe0f44e3a564b36043a4",
     "5574272505b827ecfc1720b9f839a4c40467e9fd35fb051c26e52c92b58dbc90",
     "336feb1d24578af255ac174fe32f0e0e18f9f11048aca700e27efec07a546023",
     "0b713a67d0586de018d7ca3583c19e2b61e53b1dc70c957c07c0bb9649929daf"},
    {"beautiful_mermaid_0f02dc158bb5fa8b",
     "0f02dc158bb5fa8bc1f0d991d40a2495b8208c3ec399909155d4f3c5b6e7ba27",
     "0301a282a95ac7f89a9e205e44b0bf6d09be2c8b3f6dd4c9fceb01dba1571a11",
     "68bb0d9ba461ffbd73e89cca69a9a26817452839a5709fe2eb62af0c6fe365b4",
     "48c95c953133dfaf375cb13d2071d76bde7e2aa49fc68e1342e6b60c9e315bd5"},
    {"beautiful_mermaid_aea8f60cfe1ecdeb",
     "aea8f60cfe1ecdeb97d9c60a2bfe048b08fa0c5f8f96883a5c7a7f0fdc08ade1",
     "52168ae6f29847f40b44cbb26252f6ed6e914be22a9f650fd5a92d018527f172",
     "2de4ab3648d1eab5b6ff5d29d8105d2f4c5c3f5bfa8d18ca41e79f6396d78e9f",
     "8955e45bba0e03d4802d58869b8e55d71a1787fa81efc490f437ef888b8b1c23"},
    {"beautiful_mermaid_ea7675b95cb763b3",
     "ea7675b95cb763b31fae93f37ab7fefe9e427ffe137ab9275351afd30511ff27",
     "9dd4fd29c71283d8f1abe79a4ecdb9f53d9f7a1d8876b2d6fb58d9237da096e7",
     "f0376271d7c50202cf11f21b66ef15c08d69a1968fa56c11d43a68209b09a785",
     "d2cbb27da6812a0b0890b615fd12f0d536fc260138ddcf63f91feda490a11711"},
    {"beautiful_mermaid_0e7a645593f70a3a",
     "0e7a645593f70a3a7291f554375c88b7f6a9d94e480d1c29074c7562c005981d",
     "a3891473659f4102921ee042b6b85dc033e048fc536d7f0fb48f7fa447925847",
     "0b43d84220eaaef58696872c56f077ceb1e6727e127f5dc2cbf229c57da230d8",
     "d3ae8f8ed45a330d918ba148d4c35d4421a682d73319d9f235ac0b77d4291246"},
    {"beautiful_mermaid_e4a085e0bd0b6697",
     "e4a085e0bd0b6697e9370b9edb760625683047c2802e89ecb86730579208b96b",
     "63db01332f0230cb70085d39ff9e76c2633cd7423a05e0ece4f0cbde8f51618d",
     "700ab72195957b0a83a32331b75563d820fb12c6aac88086340b87374c16e007",
     "3ac2b77efcdc1c654115a2e88a2f4a917671d3bd5cf76962eada66b1a5dc3d5a"},
    {"beautiful_mermaid_cc0fb78b963c854d",
     "cc0fb78b963c854dbb47f0990f7312a9ec778c3fa47aca1f1782fd4dd53edcf3",
     "cec41794e6464836a52e62c5f40252c2e62fe5bcb23f5416bd9d59ebcf1669c2",
     "eb5ed9b419785b2f2d560a909477e5b116a49b5f007c8a61a0b7e7c64cdad22a",
     "033b46f75bad17702587d282fcdab75ef1221675daf430a01ed339522f38305e"},
    {"beautiful_mermaid_6a5ecc7333e7c228",
     "6a5ecc7333e7c228c446728cf81a8842810c599a4393a895aa95e9f0797ded46",
     "3eec11e52bfc0d070f009b36f1db44353792660a12d7228fe020865c7d58ae2d",
     "c8291e8f84f00f6e018dd4aece105b54a0fd94c25d720d46308beb5602186025",
     "291a69222038dfc6a15b0817bae8d34fdcec8e492fa854a0571f29682c38f223"},
    {"beautiful_mermaid_0f8bc6c27ba66a25",
     "0f8bc6c27ba66a253a6fba877006d560a7cf649865c5733e6529bf816396e58b",
     "20c8be485749ffbcc4259b222bb59b30b23ebfcfeab0e1514c4fd40c08af7594",
     "3f0150e5696d0f4ef7435ed3f6f26a12aaf895ebbd9d052f2504971ee6add7a8",
     "f1812eeab85a1209bee239a5eb3403b7614db229f95912ad585d969ef3a5ce8c"},
    {"beautiful_mermaid_784b5100a057e9cc",
     "784b5100a057e9ccd66035bff87e1acf54c65eb85bf754127289dfd7a6e4cdb5",
     "ff40623f353e71e4bdab8c0923045d500be8005bf3878fb420557a5186a0a481",
     "ab1e98feef14ea396dbd84125a6189d6d9c8e2071b4c1140c102bac8e0e10eb4",
     "14d1f5a9e7949e4e110db8b6b885ddc87b8b3209bcde3606463c546341b5a7c2"},
    {"beautiful_mermaid_3df1fe30819d283a",
     "3df1fe30819d283a7c8adc7356b014d91879409e65be2a9b931ff8e8c3b2d611",
     "ec91954d8c0e6ff33d0b9242da111649ee79c953e8eece83ce25dac74ca6ba2d",
     "3e3981472ff8f419c40306ef5407eb24a570a72947cb6874d02c8c38ea727549",
     "609c795c5ebf8bc52b0b13411b856e56ffa683da190581327d13ca50c87fcd5f"},
    {"beautiful_mermaid_c5d448d8d81b800c",
     "c5d448d8d81b800c7a30246b81ccd624be1a489f991f8816e30ed71d79910e7b",
     "8cb823f23d01f7b344615ccd3dea91db6f3471e10c7b9268bfd8cf5280d538a2",
     "861c303a567f75148b435331fd594afcbe1bc55b4a1c632f76baebb02908c31a",
     "e092cc3294b83701b7ff4cb96c81457d833b64dcf51695b90efbac33b120ab18"},
    {"beautiful_mermaid_f9af27ead5bec480",
     "f9af27ead5bec480becc3f0be3c53eef1ba898b47fab7eea974e22e577c8b017",
     "de223d0cbc772d39f8a9bfff9f94a2424448bcc0a978c122e818f838612878b5",
     "aab9a216da41b62e4cac1345ee12833043df48784af521465004b429dc703ec5",
     "98cc9706c98233aeb0ee0ef11b6fcfdd81f0e09789666a7d00038931e9b25c40"},
    {"beautiful_mermaid_355b11988fd4e567",
     "355b11988fd4e5674eee563961d4b45c57d015ac934023ae660baf8b9a21f11e",
     "b838eb381268bb86f40503c4e7b61201879a0fbe9d3ffe33b76f9c234b29eeca",
     "4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945",
     "3d6dc510d11f0508ccb4590348ba275961194df20deaca1cef7db70a06ea5d53"},
    {"beautiful_mermaid_e3c12bf13dcf37bd",
     "e3c12bf13dcf37bd1f6816962859940e13e4b8678265711dbec3264052ae0fee",
     "a1eb5bdaace3a14ab31605d3338c697863ab5a7f4e571c45a53c27981dbec357",
     "5fd8d88d3e15da9a37a2c2d7990cbcf0e81962e6cea10429f87026f40359c040",
     "5e14bd19ae49176ade025146f989de1182c059804f4f083aef8ca4fbd8c08033"},
    {"beautiful_mermaid_8253ecd344b35e51",
     "8253ecd344b35e51f431a17c4f8ad62068b5965e857aef004003090bd4a6c472",
     "6aac16a407e3b1ef92a7c4e7f3414bb8fde8b751132aaff7c86381bb66c23c9d",
     "ad0cd8a98ca80b0163fde86e1e5edf10f69c796149b6410ea1ff2eb7a9188047",
     "3187ad24b1bca9d645e0d637b5be18b315a106a9b70da31f8d694f93dc693da2"},
}};

std::string read_cases() {
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_terminal/beautiful_mermaid/cases.json";
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read Beautiful Mermaid input inventory");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void check_frozen(JsonVal cases) {
    std::set<std::string> actual;
    cases.iter([&](JsonVal row) {
        if (row.get_string("family") != "flowchart") return;
        const auto id = row.get_string("id");
        const auto expected =
            std::ranges::find_if(kFrozen, [&](const FrozenCase& f) { return id == f.id; });
        if (expected == kFrozen.end() || !actual.insert(id).second)
            throw std::runtime_error("Changed/duplicate frozen flowchart identity");
        const auto expectation = row.get("expectation");
        if (digest(row.get_string("input")) != expected->input ||
            row.get_string("sha256") != expected->input ||
            digest(expectation.get("semantic_model").to_string()) != expected->model ||
            digest(expectation.get("assertions").to_string()) != expected->assertions ||
            digest(row.get("provenance").to_string()) != expected->provenance ||
            expectation.get_string("classification") != "drawing")
            throw std::runtime_error("Frozen input/model/assertion/provenance changed: " + id);
    });
    if (actual.size() != kFrozen.size()) throw std::runtime_error("Missing frozen flowchart input");
}

std::string fenced(std::string_view source, std::string_view language) {
    return "```" + std::string(language) + "\n" + std::string(source) + "\n```\n\n" +
           std::string(kSentinel);
}

std::vector<std::string> portable_assertions(const Grid& grid, JsonVal assertions) {
    std::vector<std::string> errors;
    std::string text;
    for (int y = 0; y < grid.height; ++y)
        text += grid.row(y) + '\n';
    assertions.iter([&](JsonVal a) {
        if (a.get("render_options").get("useAscii").as_bool()) return;
        const auto kind = a.get_string("kind"), value = a.get_string("value");
        if (kind != "contains" && kind != "not_contains") {
            errors.emplace_back("Unported original assertion kind");
            return;
        }
        const bool contains = text.find(value) != std::string::npos;
        if ((kind == "contains") != contains)
            errors.emplace_back("Original Unicode predicate failed: " + kind + " " + value);
    });
    return errors;
}

// An upstream implementation defect is retained in the frozen vendor model
// and golden. This separately locked source correction states Mermaid meaning;
// no observed Loom cells are used to establish these membership facts.
constexpr std::string_view kMembershipCorrection =
    R"CORRECTION({"case_id":"beautiful_mermaid_eead33eba3471656","source_sha256":"eead33eba347165699f77de19b7d5d44d69d77a5d4e5fac399b2f576e08df501","reason":"Pinned Beautiful Mermaid parser omits group membership when nodes were declared before a subgraph. Preserve original model/golden; use independently reviewed Mermaid source membership.","witnesses":[{"offset":62,"text":"subgraph Frontend\nA\nend"},{"offset":86,"text":"subgraph Backend\nB\nC\nend"}],"membership":{"Frontend":["A"],"Backend":["B","C"]}})CORRECTION";
constexpr std::string_view kMembershipCorrectionSha =
    "780466b0ec0d3f7acdfa3b9814d4e8f001e777d2b7dd9a28b2a3f2de7d49c45e";

void apply_source_correction(std::string_view id, std::string_view source, Model& model) {
    if (id != "beautiful_mermaid_eead33eba3471656") return;
    if (digest(kMembershipCorrection) != kMembershipCorrectionSha)
        throw std::runtime_error("Source correction facts changed");
    const auto document = json::parse(kMembershipCorrection);
    if (!document || digest(source) != document->root().get_string("source_sha256"))
        throw std::runtime_error("Source correction input changed");
    document->root().get("witnesses").iter([&](JsonVal witness) {
        const auto offset = witness.get_int("offset");
        const auto text = witness.get_string("text");
        if (offset < 0 || static_cast<std::size_t>(offset) > source.size() ||
            source.substr(static_cast<std::size_t>(offset), text.size()) != text)
            throw std::runtime_error("Source correction witness changed");
    });
    for (auto& group : model.groups) {
        if (!group.nodes.empty())
            throw std::runtime_error(
                "Upstream membership omission changed; reconsider source correction");
        const auto members = document->root().get("membership").get(group.id);
        if (!members.is_arr()) throw std::runtime_error("Source correction group changed");
        members.iter([&](JsonVal member) {
            const auto found = std::ranges::find_if(
                model.nodes, [&](const Node& n) { return n.id == member.as_str(); });
            if (found == model.nodes.end())
                throw std::runtime_error("Source correction node changed");
            group.nodes.push_back(static_cast<int>(found - model.nodes.begin()));
        });
    }
}

Model simple_model() {
    return {"LR", {{"a", "A"}, {"b", "B"}, {"c", "C"}}, {{0, 1, "", "solid", false, true}}, {}};
}

Grid simple_grid() {
    return mock(
        {"┌───┐     ┌───┐     ┌───┐", "│ A ├────▶│ B │     │ C │", "└───┘     └───┘     └───┘"});
}

TEST(MermaidFlowOracle, SourceMembershipCorrectionStillRejectsWrongOwnership) {
    const auto document = json::parse(read_cases());
    ASSERT_TRUE(document.has_value());
    std::string source;
    Model model;
    document->root().get("cases").iter([&](JsonVal row) {
        if (row.get_string("id") != "beautiful_mermaid_eead33eba3471656") return;
        source = row.get_string("input");
        model = parse_model(row.get("expectation").get("semantic_model"));
    });
    ASSERT_EQ(model.groups.size(), 2u);
    ASSERT_TRUE(model.groups[0].nodes.empty());
    ASSERT_TRUE(model.groups[1].nodes.empty());
    apply_source_correction("beautiful_mermaid_eead33eba3471656", source, model);
    EXPECT_EQ(model.groups[0].nodes, (std::vector<int>{0}));
    EXPECT_EQ(model.groups[1].nodes, (std::vector<int>{1, 2}));
    const auto correct =
        mock({"┌───────────────────┐", "│Frontend           │", "│                   │",
              "│  ┌────────────┐   │", "│  │Web Server  │   │", "│  └─────┬──────┘   │",
              "│        │          │", "└────────┼──────────┘", "         │           ",
              "┌────────┼──────────┐", "│Backend │          │", "│        │          │",
              "│        ▼          │", "│  ┌────────────┐   │", "│  │API Gateway │   │",
              "│  └─────┬──────┘   │", "│        │          │", "│        │          │",
              "│        ▼          │", "│  ┌────────────┐   │", "│  │Web Server  │   │",
              "│  └────────────┘   │", "│                   │", "└───────────────────┘"});
    ASSERT_TRUE(validate(correct, model).empty()) << errors_text(validate(correct, model));
    auto wrong = model;
    wrong.groups[0].nodes = {2};
    wrong.groups[1].nodes = {0, 1};
    EXPECT_FALSE(validate(correct, wrong).empty());
    wrong = model;
    wrong.groups[0].nodes.clear();
    wrong.groups[1].nodes.clear();
    EXPECT_FALSE(validate(correct, wrong).empty());
    EXPECT_THROW(apply_source_correction("beautiful_mermaid_eead33eba3471656", source + " ", model),
                 std::runtime_error);
}

TEST(MermaidFlowOracle, Frozen66InventoryDoesNotUseRenderer) {
    const auto document = json::parse(read_cases());
    ASSERT_TRUE(document.has_value());
    EXPECT_NO_THROW(check_frozen(document->root().get("cases")));
}

TEST(MermaidFlowOracle, DetectsMissingArrowsAndBrokenShafts) {
    auto grid = simple_grid();
    const auto model = simple_model();
    ASSERT_TRUE(validate(grid, model).empty()) << errors_text(validate(grid, model));
    grid.put({9, 1}, "─");
    EXPECT_FALSE(validate(grid, model).empty());
    grid = simple_grid();
    grid.put({7, 1}, " ");
    EXPECT_FALSE(validate(grid, model).empty());
}

TEST(MermaidFlowOracle, RejectsSwappedTargetWithAllLabelsAndArrowCountPresent) {
    const auto grid = simple_grid();
    auto model = simple_model();
    ASSERT_TRUE(validate(grid, model).empty());
    model.edges.front().to = 2;
    EXPECT_FALSE(validate(grid, model).empty());
}

TEST(MermaidFlowOracle, MatchesDuplicateLabelsByDistinctFramesAndTopology) {
    auto grid = mock({"┌────┐   ┌────┐", "│Same├──▶│Same│", "└────┘   └────┘"});
    const Model model{
        "LR", {{"source", "Same"}, {"target", "Same"}}, {{0, 1, "", "solid", false, true}}, {}};
    ASSERT_TRUE(validate(grid, model).empty()) << errors_text(validate(grid, model));
    grid.put({10, 1}, "X");
    EXPECT_FALSE(validate(grid, model).empty());
}

TEST(MermaidFlowOracle, PreservesMultilineOrderingAndExplicitEmptyRows) {
    const Model model{"TD", {{"a", "First\n\nSecond"}}, {}, {}};
    const auto good = mock({"┌────────┐", "│ First  │", "│        │", "│ Second │", "└────────┘"});
    ASSERT_TRUE(validate(good, model).empty()) << errors_text(validate(good, model));
    const auto no_blank = mock({"┌────────┐", "│ First  │", "│ Second │", "└────────┘"});
    EXPECT_FALSE(validate(no_blank, model).empty());
    const auto reversed =
        mock({"┌────────┐", "│ Second │", "│        │", "│ First  │", "└────────┘"});
    EXPECT_FALSE(validate(reversed, model).empty());
    const auto flat = mock({"┌────────────┐", "│FirstSecond │", "└────────────┘"});
    EXPECT_FALSE(validate(flat, model).empty());
}

TEST(MermaidFlowOracle, RejectsWrongOrOpenGroupFramesWithoutLosingNodeLabels) {
    auto grid = mock({"┌─────────────┐      ", "│ Group       │      ", "│ ┌───┐       │ ┌───┐",
                      "│ │ A ├───────┼▶│ B │", "│ └───┘       │ └───┘", "│             │      ",
                      "└─────────────┘      "});
    const Model model{"LR",
                      {{"a", "A"}, {"b", "B"}},
                      {{0, 1, "", "solid", false, true}},
                      {{"g", "Group", "", -1, {0}}}};
    ASSERT_TRUE(validate(grid, model).empty()) << errors_text(validate(grid, model));
    auto wrong = model;
    wrong.groups.front().nodes = {1};
    EXPECT_FALSE(validate(grid, wrong).empty());
    grid.put({0, 5}, " ");
    EXPECT_FALSE(validate(grid, model).empty());
}

TEST(MermaidFlowOracle, RetainsNestedMembershipAndSiblingSeparation) {
    const auto grid = mock({"┌─────────────────┐", "│ Outer           │", "│ ┌─────────────┐ │",
                            "│ │ Inner       │ │", "│ │ ┌───┐       │ │", "│ │ │ A │       │ │",
                            "│ │ └───┘       │ │", "│ │             │ │", "│ └─────────────┘ │",
                            "│                 │", "└─────────────────┘"});
    Model model{
        "LR", {{"a", "A"}}, {}, {{"outer", "Outer", "", -1, {}}, {"inner", "Inner", "", 0, {0}}}};
    ASSERT_TRUE(validate(grid, model).empty()) << errors_text(validate(grid, model));
    model.groups[1].parent = -1;
    EXPECT_FALSE(validate(grid, model).empty());
}

TEST(MermaidFlowOracle, ChecksDottedAndThickShaftIdentity) {
    auto grid = simple_grid();
    auto model = simple_model();
    model.edges.front().style = "dotted";
    for (int x = 5; x <= 8; ++x)
        grid.put({x, 1}, "┄");
    ASSERT_TRUE(validate(grid, model).empty()) << errors_text(validate(grid, model));
    EXPECT_FALSE(validate(simple_grid(), model).empty());
    auto mixed = grid;
    mixed.put({6, 1}, "─");
    EXPECT_FALSE(validate(mixed, model).empty());
    mixed = simple_grid();
    mixed.put({5, 1}, "┄");
    EXPECT_FALSE(validate(mixed, model).empty());
    model.edges.front().style = "thick";
    for (int x = 5; x <= 8; ++x)
        grid.put({x, 1}, "━");
    EXPECT_TRUE(validate(grid, model).empty()) << errors_text(validate(grid, model));
    mixed = grid;
    mixed.put({6, 1}, "─");
    EXPECT_FALSE(validate(mixed, model).empty());
    mixed = simple_grid();
    mixed.put({5, 1}, "━");
    EXPECT_FALSE(validate(mixed, model).empty());
}

TEST(MermaidFlowOracle, RejectsJunctionShaftBypassButAllowsRealCornersAndGroupCrossings) {
    auto solid = simple_grid();
    auto model = simple_model();
    for (int x = 5; x <= 8; ++x)
        solid.put({x, 1}, "┼");
    EXPECT_FALSE(validate(solid, model).empty());
    model.edges[0].style = "dotted";
    solid.put({5, 1}, "┄");
    EXPECT_FALSE(validate(solid, model).empty());
    const auto corner = mock({"┌───┐", "│ A ├┄┐", "└───┘ ┆", "      ┆", "      ┆ ┌───┐",
                              "      └▶│ B │", "        └───┘"});
    const Model two{"LR", {{"a", "A"}, {"b", "B"}}, {{0, 1, "", "dotted", false, true}}, {}};
    ASSERT_TRUE(validate(corner, two).empty()) << errors_text(validate(corner, two));
    auto crossing = mock({"┌─────────────┐      ", "│ Group       │      ", "│ ┌───┐       │ ┌───┐",
                          "│ │ A ├───────┼▶│ B │", "│ └───┘       │ └───┘", "│             │      ",
                          "└─────────────┘      "});
    for (int x = 7; x < 14; ++x)
        crossing.put({x, 3}, "┄");
    const Model grouped{"LR",
                        {{"a", "A"}, {"b", "B"}},
                        {{0, 1, "", "dotted", false, true}},
                        {{"g", "Group", "", -1, {0}}}};
    ASSERT_TRUE(validate(crossing, grouped).empty()) << errors_text(validate(crossing, grouped));
}

TEST(MermaidFlowOracle, RejectsOrphanRoutesAndExtraUnheadedParallelEdges) {
    const auto good = simple_grid();
    const auto model = simple_model();
    ASSERT_TRUE(validate(good, model).empty());
    auto orphan = good;
    for (int x = 6; x < 9; ++x)
        orphan.put({x, 0}, "─");
    EXPECT_FALSE(validate(orphan, model).empty());
    orphan = good;
    orphan.put({7, 0}, "┼");
    EXPECT_FALSE(validate(orphan, model).empty());
    orphan = good;
    orphan.put({0, 1}, "┼");
    EXPECT_FALSE(validate(orphan, model).empty());
    const auto parallel = mock({"┌───┐       ┌───┐", "│ A ├──────▶│ B │", "│   │       │   │",
                                "│   ├───────┤   │", "└───┘       └───┘"});
    const Model two{"LR", {{"a", "A"}, {"b", "B"}}, {{0, 1, "", "solid", false, true}}, {}};
    EXPECT_FALSE(validate(parallel, two).empty());
}

TEST(MermaidFlowOracle, RejectsUnclaimedOrDuplicatedNodeHeaderAndLeafText) {
    const Model one{"LR", {{"a", "A"}}, {}, {}};
    const auto good = mock({"┌─────────┐", "│ A       │", "└─────────┘"});
    ASSERT_TRUE(validate(good, one).empty()) << errors_text(validate(good, one));
    EXPECT_FALSE(validate(mock({"┌─────────┐", "│ A A     │", "└─────────┘"}), one).empty());
    EXPECT_FALSE(validate(mock({"┌─────────┐", "│ A extra │", "└─────────┘"}), one).empty());
    const auto extra =
        mock({"┌───┐     ┌───┐     ┌───┐     ┌───┐", "│ A ├────▶│ B │     │ C │     │ D │",
              "└───┘     └───┘     └───┘     └───┘"});
    EXPECT_FALSE(validate(extra, simple_model()).empty());
    auto group = mock({"┌─────────────┐      ", "│ Group       │      ", "│ ┌───┐       │ ┌───┐",
                       "│ │ A ├───────┼▶│ B │", "│ └───┘       │ └───┘", "│             │      ",
                       "└─────────────┘      "});
    const Model grouped{"LR",
                        {{"a", "A"}, {"b", "B"}},
                        {{0, 1, "", "solid", false, true}},
                        {{"g", "Group", "", -1, {0}}}};
    ASSERT_TRUE(validate(group, grouped).empty());
    group.put({10, 1}, "X");
    EXPECT_FALSE(validate(group, grouped).empty());
}

TEST(MermaidFlowOracle, RetainsCompleteEdgeLabelsOnTheirOwnConnectedRoutes) {
    auto grid = mock({"┌───┐           ┌───┐", "│ A ├─tag──────▶│ B │", "└───┘           └───┘"});
    Model model{"LR", {{"a", "A"}, {"b", "B"}}, {{0, 1, "tag", "solid", false, true}}, {}};
    ASSERT_TRUE(validate(grid, model).empty()) << errors_text(validate(grid, model));
    grid.put({10, 0}, "X");
    EXPECT_FALSE(validate(grid, model).empty());
    model.edges[0].label = "other";
    EXPECT_FALSE(validate(grid, model).empty());
}

Grid wrap_group(const Grid& inner, std::string_view title) {
    Grid out{inner.width + 4, inner.height + 4,
             std::vector<std::string>(
                 static_cast<std::size_t>((inner.width + 4) * (inner.height + 4)), " ")};
    for (int x = 1; x < out.width - 1; ++x) {
        out.put({x, 0}, "─");
        out.put({x, out.height - 1}, "─");
    }
    for (int y = 1; y < out.height - 1; ++y) {
        out.put({0, y}, "│");
        out.put({out.width - 1, y}, "│");
    }
    out.put({0, 0}, "┌");
    out.put({out.width - 1, 0}, "┐");
    out.put({0, out.height - 1}, "└");
    out.put({out.width - 1, out.height - 1}, "┘");
    int x = 2;
    for (const auto& value : ftxui::Utf8ToGlyphs(std::string(title)))
        out.put({x++, 1}, value);
    for (int y = 0; y < inner.height; ++y)
        for (int at = 0; at < inner.width; ++at)
            out.put({at + 2, y + 2}, std::string(inner.at({at, y})));
    return out;
}

TEST(MermaidFlowOracle, ResolvesNearestExplicitAncestorThenGlobalDirection) {
    const auto grid = mock({"┌─────────┐", "│ Group   │", "│ ┌───┐   │", "│ │ A │   │",
                            "│ └─┬─┘   │", "│   │     │", "│   ▼     │", "│ ┌───┐   │",
                            "│ │ B │   │", "│ └───┘   │", "└─────────┘"});
    Model model{"TD",
                {{"a", "A"}, {"b", "B"}},
                {{0, 1, "", "solid", false, true}},
                {{"g", "Group", "", -1, {0, 1}}}};
    ASSERT_TRUE(validate(grid, model).empty()) << errors_text(validate(grid, model));
    model.direction = "LR";
    EXPECT_FALSE(validate(grid, model).empty());
    model.groups[0].direction = "TD";
    EXPECT_TRUE(validate(grid, model).empty());
    const auto wrapped = wrap_group(grid, "Outer");
    model.groups = {{"outer", "Outer", "TD", -1, {}}, {"g", "Group", "", 0, {0, 1}}};
    EXPECT_TRUE(validate(wrapped, model).empty()) << errors_text(validate(wrapped, model));
    model.groups[0].direction = "LR";
    EXPECT_FALSE(validate(wrapped, model).empty());
}

TEST(MermaidFlowOracle, BoundsDenseGeometryAndExcessArrowMutations) {
    const Model model{"LR", {{"a", "A"}}, {}, {}};
    const Grid heads{40, 40, std::vector<std::string>(1600, "▶")};
    const auto too_many_heads = validate(heads, model);
    ASSERT_FALSE(too_many_heads.empty());
    EXPECT_NE(too_many_heads.front().find("Extra arrowheads"), std::string::npos);
    const Grid dense{40, 40, std::vector<std::string>(1600, "┼")};
    const auto too_many_frames = validate(dense, model);
    ASSERT_FALSE(too_many_frames.empty());
    EXPECT_NE(too_many_frames.front().find("Too many candidate"), std::string::npos);
}

TEST(MermaidFlowOracle, UsesOneEntryPerWideCellAndPreservesCjkNodeTopology) {
    const auto text_cells = capture(ftxui::text("中文"), 4, 1);
    EXPECT_TRUE(text_cells == mock({"中文"}));
    auto scene = mock({"┌────┐   ┌────┐   ┌────┐", "│ 源 │   │ 目 │   │ 旁 │",
                       "│ 点 ├──▶│ 标 │   │    │", "└────┘   └────┘   └────┘"});
    const Model model{"LR",
                      {{"source", "源\n点"}, {"target", "目\n标"}, {"other", "旁"}},
                      {{0, 1, "", "solid", false, true}},
                      {}};
    ASSERT_EQ(scene.cells.size(), static_cast<std::size_t>(scene.width * scene.height));
    ASSERT_TRUE(validate(scene, model).empty()) << errors_text(validate(scene, model));
    auto wrong_row = scene;
    wrong_row.put({2, 1}, "点");
    wrong_row.put({2, 2}, "源");
    EXPECT_FALSE(validate(wrong_row, model).empty());
    auto wrong_target = model;
    wrong_target.edges[0].to = 2;
    EXPECT_FALSE(validate(scene, wrong_target).empty());
    auto missing = scene;
    missing.put({11, 2}, " ");
    EXPECT_FALSE(validate(missing, model).empty());
}

TEST(MermaidFlowOracle, PreservesCjkEdgeLabelContinuationAndAsciiTail) {
    const auto scene =
        mock({"┌───┐            ┌───┐", "│ A ├─请求x─────▶│ B │", "└───┘            └───┘"});
    const Model model{"LR", {{"a", "A"}, {"b", "B"}}, {{0, 1, "请求x", "solid", false, true}}, {}};
    ASSERT_TRUE(validate(scene, model).empty()) << errors_text(validate(scene, model));
    auto missing = scene;
    missing.put({10, 1}, " ");
    EXPECT_FALSE(validate(missing, model).empty());
    auto broken_continuation = scene;
    broken_continuation.put({7, 1}, "X");
    EXPECT_FALSE(validate(broken_continuation, model).empty());
}

TEST(MermaidFlowSemantics, FrozenBeautifulFlowchartBatch) {
    const auto document = json::parse(read_cases());
    ASSERT_TRUE(document.has_value());
    ASSERT_NO_THROW(check_frozen(document->root().get("cases")));
    std::size_t visited = 0;
    json::JsonMutDoc report;
    auto report_root = report.object();
    auto report_cases = report_root.make_arr();
    report_root.set("kind", "frozen_beautiful_flowchart_semantic_observations");
    report_root.set("qualification",
                    "Expectations are frozen upstream source/model/assertions; captured Loom cells "
                    "are observations, never expectation updates.");
    document->root().get("cases").iter([&](JsonVal item) {
        if (item.get_string("family") != "flowchart") return;
        ++visited;
        SCOPED_TRACE(item.get_string("id"));
        const auto source = item.get_string("input");
        const auto expectation = item.get("expectation");
        auto model = parse_model(expectation.get("semantic_model"));
        apply_source_correction(item.get_string("id"), source, model);
        auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
        element->ComputeRequirement();
        const int height = std::min(
            1040,
            std::max({32, element->requirement().min_y + 8,
                      static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 12}));
        const auto wide = capture(element, 240, height), plain = capture(ordinary, 240, height);
        EXPECT_FALSE(wide == plain) << "Positive drawing unexpectedly returned source";
        std::vector<std::string> assignments;
        const auto errors = validate(wide, model, &assignments);
        EXPECT_TRUE(errors.empty()) << errors_text(errors);
        EXPECT_TRUE(portable_assertions(wide, expectation.get("assertions")).empty())
            << errors_text(portable_assertions(wide, expectation.get("assertions")));
        const auto narrow = capture(element, 40, height);
        int content_width = 0;
        for (int y = 0; y < height; ++y) {
            if (wide.row(y).find(kSentinel) != std::string::npos) break;
            for (int x = 0; x < wide.width; ++x)
                if (wide.at({x, y}) != " " && !wide.at({x, y}).empty())
                    content_width = std::max(content_width, x + 1);
        }
        if (content_width > 40) {
            EXPECT_TRUE(narrow == capture(ordinary, 40, height))
                << "Narrow view must retain exact ordinary source";
        } else {
            EXPECT_TRUE(validate(narrow, model).empty()) << errors_text(validate(narrow, model));
        }
        const auto restored = capture(element, 240, height);
        EXPECT_TRUE(restored == wide) << "Same Node did not restore its original cells";
        auto sentinel_visible = [&](const Grid& g) {
            for (int y = 0; y < g.height; ++y)
                if (g.row(y).find(kSentinel) != std::string::npos) return true;
            return false;
        };
        EXPECT_TRUE(sentinel_visible(wide));
        EXPECT_TRUE(sentinel_visible(narrow));
        EXPECT_TRUE(sentinel_visible(restored));
        auto row = report_root.make_obj();
        row.set("id", item.get_string("id"));
        row.set("input_sha256", digest(source));
        row.set("semantic_model_sha256", digest(expectation.get("semantic_model").to_string()));
        row.set("source_semantic_correction_sha256",
                item.get_string("id") == "beautiful_mermaid_eead33eba3471656"
                    ? kMembershipCorrectionSha
                    : std::string_view{});
        row.set("source_fallback", wide == plain);
        row.set("restored_identically", restored == wide);
        row.set("content_width", content_width);
        auto append_strings = [&](std::string_view key, const std::vector<std::string>& values) {
            auto list = report_root.make_arr();
            for (const auto& value : values)
                list.append(report_root.make_str(value));
            row.add(key, list);
        };
        append_strings("semantic_errors", errors);
        append_strings("portable_assertion_errors",
                       portable_assertions(wide, expectation.get("assertions")));
        append_strings("candidate_assignment_diagnostics", assignments);
        auto cells = [&](std::string_view key, const Grid& grid) {
            std::vector<std::string> lines;
            for (int y = 0; y < grid.height; ++y)
                lines.push_back(grid.row(y));
            append_strings(key, lines);
        };
        cells("wide_frame", wide);
        cells("narrow_frame", narrow);
        cells("restored_frame", restored);
        report_cases.append(row);
    });
    EXPECT_EQ(visited, 66u);
    report_root.add("cases", report_cases);
    report.set_root(report_root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto path =
        std::filesystem::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-flow-semantics.json";
    std::ofstream output(path, std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_pretty_string() << '\n')) << "Cannot write " << path;
#endif
}

struct OfficialExpansion {
    const char* path;
    const char* body;
    const char* model;
};

constexpr std::array<OfficialExpansion, 16> kOfficialExpansion{{
    {"flowchart/nightly-check/dagre/57-dagre-handle-nested-subgraphs-with-outgoing-links-4.mmd",
     "c4ef7a898772a55de82aeac6da3448f603d7a4f0bfcfa24d56774b6d61ef3187",
     "a3817190c0ec91fff485d46e1c00be458b86a3ffe0d6508ff873af84da9ae48d"},
    {"flowchart/nightly-check/dagre/6088-2-should-handle-diamond-shape-intersections.mmd",
     "41cd70cc543587aa5ae6c5960b40759b08c88146db8fa321b86e2f8da262f5ce",
     "2b5f30730c86c132677f470a798a9296a3a10ef673039fdfefec6dea1c98f6c2"},
    {"flowchart/nightly-check/dagre/6088-5-should-handle-diamond-shape-intersections.mmd",
     "3369b204c1740684cac50124e69a05159bea766b79f82ff1c513fc62fd371666",
     "60c317c73775fa9b895ee14127d1be670775754dd68e85603b0cccce166dbf7d"},
    {"flowchart/nightly-check/dagre/"
     "6647-dagre-should-keep-node-order-when-using-dagre-layout-unless-it-would-add-crossings.mmd",
     "409efb975de0d8aa3e2dc3802d4c6e4a7e969a9ea81ecb25bc4f4e9905a449b4",
     "29c5a8cc4acd286b04c36bb49c8eb2cf520d3f92885d1aae5de3110b9fe07b76"},
    {"flowchart/nightly-check/dagre/"
     "dagre-should-merge-edges-within-subgraphs-when-dagre-mergeedges-is-true-7659.mmd",
     "0db955656f12fa2f0be78ab27be77fad77046b482b7fd66b3685ea219b2cfc06",
     "241f9bb71afeb8ffc04ddfac55f9c913fe296786d14e81d75eb201626d0b6e83"},
    {"flowchart/nightly-check/handdrawn/multiple-nodes-and-chaining-in-one-statement.mmd",
     "f7c90b898b2873d8a707ff11b6a2a89f0f4c6229c5ab6bdbe39d40c9b407c860",
     "24e49543087fa552d458b6c84ba6fb1728ac96db09c37b38f4525e04ae45e541"},
    {"flowchart/nightly-check/handdrawn/should-render-subgraphs.mmd",
     "df6ba2b0a35d75b92c646eeaa54abc6529d158e3772b8acc8a1109e00e7de785",
     "a53be8b370bfc49ac73f651896c3268c4421e844d3e73a311c154c961ff0944b"},
    {"flowchart/nightly-check/redux-color-subgraphs/"
     "02-should-render-nested-subgraph-containers.mmd",
     "76088f4d148c21f8e72cfa4d60b0b1f46af7358986cb37f4e5fe2f8eb2548bfd",
     "d4518ed30582b9485232c7e5e04c77b1a20d46e5832aabdee135893486d6e794"},
    {"flowchart/nightly-check/redux-color-subgraphs/"
     "06-should-render-nested-subgraph-containers.mmd",
     "76088f4d148c21f8e72cfa4d60b0b1f46af7358986cb37f4e5fe2f8eb2548bfd",
     "d4518ed30582b9485232c7e5e04c77b1a20d46e5832aabdee135893486d6e794"},
    {"flowchart/nightly-check/redux-color-subgraphs/"
     "10-should-render-nested-subgraph-containers.mmd",
     "76088f4d148c21f8e72cfa4d60b0b1f46af7358986cb37f4e5fe2f8eb2548bfd",
     "d4518ed30582b9485232c7e5e04c77b1a20d46e5832aabdee135893486d6e794"},
    {"flowchart/nightly-check/redux-color-subgraphs/"
     "14-should-render-nested-subgraph-containers.mmd",
     "76088f4d148c21f8e72cfa4d60b0b1f46af7358986cb37f4e5fe2f8eb2548bfd",
     "d4518ed30582b9485232c7e5e04c77b1a20d46e5832aabdee135893486d6e794"},
    {"flowchart/visual-check/19-multiple-nodes-and-chaining-in-one-statement.mmd",
     "f7c90b898b2873d8a707ff11b6a2a89f0f4c6229c5ab6bdbe39d40c9b407c860",
     "24e49543087fa552d458b6c84ba6fb1728ac96db09c37b38f4525e04ae45e541"},
    {"flowchart/visual-check/57-handle-nested-subgraphs-with-outgoing-links-4.mmd",
     "c4ef7a898772a55de82aeac6da3448f603d7a4f0bfcfa24d56774b6d61ef3187",
     "a3817190c0ec91fff485d46e1c00be458b86a3ffe0d6508ff873af84da9ae48d"},
    {"flowchart/visual-check/70-should-render-a-subgraph-with-direction-td.mmd",
     "12c5199613787397241b6c645729a9e8ff9c6eba32d13a1ab39fc8a7382ee9f0",
     "994aa7bbb305561910d3e2da1edd4677480a88ad66f9fec5ab400bf78815d0dc"},
    {"flowchart/visual-check/9-should-render-subgraphs.mmd",
     "df6ba2b0a35d75b92c646eeaa54abc6529d158e3772b8acc8a1109e00e7de785",
     "a53be8b370bfc49ac73f651896c3268c4421e844d3e73a311c154c961ff0944b"},
    {"flowchart/visual-check/should-render-self-loops-in-non-tb-directions.mmd",
     "9105282d876246e5676dce644576bff95f2b1f824ddf335c1d653627dab86c78",
     "a42d78351f4ff019473527d502d46eb1427d79a8ee275abc66f708d7a6475411"},
}};

TEST(MermaidFlowSemantics, FrozenOfficialRectangleExpansion) {
    const auto root = std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
    auto read = [&](const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot read " + path.string());
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    };
    const auto document = json::parse(read(root / "manifest.json"));
    ASSERT_TRUE(document.has_value());
    std::set<std::string> visited;
    std::size_t rectangular = 0, decision = 0;
    document->root().get("fixtures").iter([&](JsonVal fixture) {
        const auto path = fixture.get_string("path");
        const auto frozen = std::ranges::find_if(
            kOfficialExpansion, [&](const OfficialExpansion& f) { return path == f.path; });
        if (frozen == kOfficialExpansion.end()) return;
        SCOPED_TRACE(path);
        ASSERT_TRUE(visited.insert(path).second);
        const auto expected = fixture.get("diagnostic");
        ASSERT_EQ(expected.get_string("expected"), "drawn");
        const auto original = read(root / "cases" / path);
        const auto offset = expected.get_int("offset");
        ASSERT_GE(offset, 0);
        ASSERT_LT(static_cast<std::size_t>(offset), original.size());
        const auto source = original.substr(static_cast<std::size_t>(offset));
        ASSERT_EQ(digest(source), frozen->body);
        const auto semantic = expected.get("cell_oracle_model");
        ASSERT_EQ(digest(semantic.to_string()), frozen->model);
        bool only_rectangles = true;
        semantic.get("nodes").iter([&](JsonVal pair) {
            if (pair.at(1).get_string("shape") != "rectangle") only_rectangles = false;
        });
        if (!only_rectangles) {
            ++decision;
            return;
        }
        ++rectangular;
        const auto model = parse_model(semantic);
        auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
        element->ComputeRequirement();
        const int height = std::min(1040, std::max(32, element->requirement().min_y + 8));
        const auto wide = capture(element, 240, height);
        EXPECT_FALSE(wide == capture(ordinary, 240, height))
            << "Positive original-body drawing obligation returned source";
        std::vector<std::string> assignments;
        const auto errors = validate(wide, model, &assignments);
        EXPECT_TRUE(errors.empty())
            << errors_text(errors)
            << " first candidate: " << (assignments.empty() ? "none" : assignments.front());
    });
    EXPECT_EQ(visited.size(), 16u);
    EXPECT_EQ(rectangular, 14u);
    EXPECT_EQ(decision, 2u);
}

TEST(MermaidFlowSemantics, OfficialInlineShapeConfigSourcesOwnCompleteGeometry) {
    struct Case {
        std::string_view path, sha;
        Model model;
    };

    const std::array cases{
        Case{"flowchart/visual-check/should-render-a-diamond-shaped-node-using-shape-config.mmd",
             "10a38ba8df398ccaf36e51e7f84dccc2a71af21c005e177d30d2c304ea209890",
             {"BT",
              {{"n2", "Untitled Node", Shape::Rectangle}, {"n1", "Diamond", Shape::Decision}},
              {{0, 1, "", "solid", false, true}},
              {}}},
        Case{"flowchart/visual-check/should-render-a-rounded-rectangle-and-a-normal-rectangle.mmd",
             "1384844adca4511a7a6f7917ebd6a9563d7dc877e5a4208f7acfc5e32abafacb",
             {"BT",
              {{"n2", "Untitled Node", Shape::Rectangle},
               {"n1", "Rounded Rectangle", Shape::Rounded},
               {"n3", "Untitled Node", Shape::Rectangle}},
              {{0, 1, "", "solid", false, true}, {2, 1, "", "solid", false, true}},
              {}}},
        Case{"flowchart/visual-check/should-render-a-stadium-shaped-node.mmd",
             "3497ab671f384701e964e3b71127e46ad889c6b67eb4fd18576bbc5a58309b8e",
             {"TB",
              {{"A", "Start", Shape::Stadium},
               {"n1", "Untitled Node", Shape::Rectangle},
               {"n2", "Untitled Node", Shape::Rectangle}},
              {{0, 1, "", "solid", false, true}, {0, 2, "", "solid", false, true}},
              {}}},
    };
    for (const auto& item : cases) {
        SCOPED_TRACE(item.path);
        const auto source =
            flow400_read(std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" /
                         std::string(item.path));
        ASSERT_EQ(digest(source), item.sha) << "Pinned complete official source changed";
        auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
        element->ComputeRequirement();
        const int height = std::min(1040, std::max(32, element->requirement().min_y + 8));
        const auto wide = capture(element, 240, height);
        const auto narrow = capture(element, 40, height);
        const auto restored = capture(element, 240, height);
        const auto plain_wide = capture(ordinary, 240, height);
        const auto plain_narrow = capture(ordinary, 40, height);
        EXPECT_FALSE(wide == plain_wide) << "Official shape source fell back to source";
        EXPECT_TRUE(restored == wide) << "Restored render changed after narrow view";
        auto geometry_errors = [&](const Grid& grid) {
            OracleWork work{kFlow400SceneLimit};
            return flow400_geometry(grid, item.model, work);
        };
        EXPECT_TRUE(geometry_errors(wide).empty());
        int content_width = 0;
        for (int y = 0; y < wide.height; ++y) {
            if (wide.row(y).find(kSentinel) != std::string::npos) break;
            for (int x = 0; x < wide.width; ++x)
                if (!wide.at({x, y}).empty() && wide.at({x, y}) != " ")
                    content_width = std::max(content_width, x + 1);
        }
        if (content_width > 40)
            EXPECT_TRUE(narrow == plain_narrow)
                << "Overwide shape source did not fall back narrowly";
        else
            EXPECT_TRUE(geometry_errors(narrow).empty())
                << "Shape geometry changed at narrow width";
        for (const auto* frame : {&wide, &narrow, &restored, &plain_wide, &plain_narrow}) {
            bool sentinel = false;
            for (int y = 0; y < frame->height; ++y)
                sentinel = sentinel || frame->row(y).find(kSentinel) != std::string::npos;
            EXPECT_TRUE(sentinel) << "Following content disappeared";
        }
    }
}

// These expectations are declared from the retained public resource policy,
// independently of renderer observations. An at-limit input can still exceed
// the separate canvas/routing budget; only compact positive boundaries below
// promise a drawing. Every rejection must preserve the complete code surface.
void check_source_policy(std::string_view source, bool expect_drawing = false,
                         const Model* semantic = nullptr) {
    auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
    element->ComputeRequirement();
    const int height = std::min(
        1040, std::max({32, element->requirement().min_y + 8,
                        static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 12}));
    const auto wide = capture(element, 240, height);
    EXPECT_EQ(wide == capture(ordinary, 240, height), !expect_drawing);
    if (semantic) {
        const auto errors = validate(wide, *semantic);
        EXPECT_TRUE(errors.empty()) << errors_text(errors);
    }
    auto has_sentinel = [&](const Grid& grid) {
        for (int y = 0; y < grid.height; ++y)
            if (grid.row(y).find(kSentinel) != std::string::npos) return true;
        return false;
    };
    EXPECT_TRUE(has_sentinel(wide));
    const auto narrow = capture(element, 40, height);
    EXPECT_TRUE(narrow == capture(ordinary, 40, height));
    EXPECT_TRUE(has_sentinel(narrow));
    const auto restored = capture(element, 240, height);
    EXPECT_TRUE(restored == wide);
    EXPECT_TRUE(has_sentinel(restored));
}

TEST(MermaidFlowSemantics, TerminalUseMaxWidthConfigRetainsCompleteDiagramGeometry) {
    constexpr std::array sources{
        std::string_view{"---\nconfig:\n  flowchart:\n    useMaxWidth: true\n---\nflowchart "
                         "TD\nA[Start] --> B[Done]\n"},
        std::string_view{"---\nconfig:\n  flowchart:\n    useMaxWidth: false\n---\nflowchart "
                         "TD\nA[Start] --> B[Done]\n"},
    };
    const Model expected{"TB",
                         {{"A", "Start", Shape::Rectangle}, {"B", "Done", Shape::Rectangle}},
                         {{0, 1, "", "solid", false, true}},
                         {}};
    for (const auto source : sources) {
        auto drawing = loom::ui::render_markdown(fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
        drawing->ComputeRequirement();
        const int height = std::max(32, drawing->requirement().min_y + 8);
        const auto wide = capture(drawing, 240, height);
        const auto narrow = capture(drawing, 40, height);
        const auto restored = capture(drawing, 240, height);
        const auto plain_wide = capture(ordinary, 240, height);
        const auto plain_narrow = capture(ordinary, 40, height);
        EXPECT_FALSE(wide == plain_wide);
        EXPECT_FALSE(narrow == plain_narrow);
        EXPECT_TRUE(restored == wide);
        OracleWork work{kFlow400SceneLimit};
        EXPECT_TRUE(flow400_geometry(wide, expected, work).empty());
        EXPECT_TRUE(flow400_geometry(narrow, expected, work).empty());
        for (const auto* frame : {&wide, &narrow, &restored}) {
            bool sentinel = false;
            for (int y = 0; y < frame->height; ++y)
                sentinel = sentinel || frame->row(y).find(kSentinel) != std::string::npos;
            EXPECT_TRUE(sentinel);
        }
    }
    check_source_policy("---\nconfig:\n  flowchart:\n    useMaxWidth: yes\n---\nflowchart "
                        "TD\nA[Start] --> B[Done]\n");
}

TEST(MermaidFlowSemantics, ZeroDiagramPaddingConfigRetainsCompleteDiagramGeometry) {
    constexpr std::string_view source =
        "---\ndescription: >-\n  Zero SVG viewport padding is inert in terminal output.\n"
        "config:\n  flowchart:\n    diagramPadding: 0\n---\n"
        "flowchart TD\nA[Start] --> B[Done]\n";
    const Model expected{"TB",
                         {{"A", "Start", Shape::Rectangle}, {"B", "Done", Shape::Rectangle}},
                         {{0, 1, "", "solid", false, true}},
                         {}};
    auto drawing = loom::ui::render_markdown(fenced(source, "mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
    drawing->ComputeRequirement();
    const int height = std::max(32, drawing->requirement().min_y + 8);
    const auto wide = capture(drawing, 240, height);
    const auto narrow = capture(drawing, 40, height);
    const auto restored = capture(drawing, 240, height);
    EXPECT_FALSE(wide == capture(ordinary, 240, height));
    EXPECT_FALSE(narrow == capture(ordinary, 40, height));
    EXPECT_TRUE(restored == wide);
    OracleWork work{kFlow400SceneLimit};
    EXPECT_TRUE(flow400_geometry(wide, expected, work).empty());
    EXPECT_TRUE(flow400_geometry(narrow, expected, work).empty());
    for (const auto* frame : {&wide, &narrow, &restored}) {
        bool sentinel = false;
        for (int y = 0; y < frame->height; ++y)
            sentinel = sentinel || frame->row(y).find(kSentinel) != std::string::npos;
        EXPECT_TRUE(sentinel);
    }
    check_source_policy("---\nconfig:\n  flowchart:\n    diagramPadding: 1\n---\nflowchart "
                        "TD\nA[Start] --> B[Done]\n");
    check_source_policy("---\nconfig:\n  flowchart:\n    diagramPadding: 0\n    diagramPadding: "
                        "0\n---\nflowchart TD\nA[Start] --> B[Done]\n");
}

TEST(MermaidFlowSemantics, TitleTopMarginConfigRequiresExactTerminalInertProfile) {
    constexpr std::array invalid{
        std::string_view{"---\nconfig:\n  layout: dagre\n  flowchart:\n    titleTopMargin: "
                         "1\n---\nflowchart TD\nA --> B\n"},
        std::string_view{"---\nconfig:\n  layout: dagre\n  flowchart:\n    titleTopMargin: 0\n    "
                         "titleTopMargin: 0\n---\nflowchart TD\nA --> B\n"},
        std::string_view{"---\nconfig:\n  layout: dagre\n  flowchart:\n    titleTopMargin: 0\n    "
                         "titleBottomMargin: 0\n---\nflowchart TD\nA --> B\n"},
        std::string_view{"---\ntitle: \"A title\"\nconfig:\n  flowchart:\n    titleTopMargin: "
                         "0\n---\nflowchart TD\nA --> B\n"},
        std::string_view{"---\ntitle: \"A title\"\nconfig:\n  layout: dagre\n  flowchart:\n    "
                         "titleTopMargin: 0\n---\nflowchart TD\nA --> B\n"},
        std::string_view{
            "---\nconfig:\n  flowchart:\n    titleTopMargin: 1\n---\nflowchart TD\nA --> B\n"},
    };
    for (const auto source : invalid)
        check_source_policy(source);
}

TEST(MermaidFlowSemantics, ShapeAliasMetadataRequiresClosedQuotedShapeAndLabel) {
    constexpr std::array invalid{
        std::string_view{"flowchart\nn0@{ shape: star, label: \"star\" }\n"},
        std::string_view{"flowchart\nn0@{ shape: event, label: event }\n"},
        std::string_view{"flowchart\nn0@{ shape: event, class: \"event\" }\n"},
        std::string_view{"flowchart\nn0@{ shape: event, label: \"event\", label: \"again\" }\n"},
        std::string_view{"flowchart\nn0[event]@{ shape: event, label: \"different\" }\n"},
        std::string_view{"flowchart\nn0@{ shape: diamond, label: \"diamond\" extra }\n"},
    };
    for (const auto source : invalid)
        check_source_policy(source);
}

TEST(MermaidFlowSemantics, DiamondShapeAliasDrawsAsDecision) {
    constexpr std::string_view source =
        "flowchart BT\n  n2[\"Untitled Node\"] --> n1@{ shape: diamond}\n";
    const Model expected{"BT",
                         {{"n2", "Untitled Node", Shape::Rectangle}, {"n1", "n1", Shape::Decision}},
                         {{0, 1, "", "solid", false, true}},
                         {}};
    auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
    element->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height =
        std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    std::array<std::optional<Grid>, 5> frames;
    OracleWork work{kFlow400SceneLimit};
    for (std::size_t n = 0; n < frames.size(); ++n)
        frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
    for (const auto& frame : frames)
        static_cast<void>(flow400_following(*frame, work, kSentinel));
    EXPECT_FALSE(flow400_same(*frames[0], *frames[3], work));
    EXPECT_TRUE(flow400_same(*frames[0], *frames[2], work));
    EXPECT_TRUE(flow400_geometry(*frames[0], expected, work, kSentinel).empty());
    if (flow400_same(*frames[1], *frames[4], work))
        SUCCEED() << "Narrow rendering uses exact ordinary fallback";
    else
        EXPECT_TRUE(flow400_geometry(*frames[1], expected, work, kSentinel).empty());
}

TEST(MermaidFlowSemantics, BareFlowchartDefaultsToTBOnlyForGroupFreeProfile) {
    constexpr std::string_view directed =
        "flowchart\nA[North source state] --> B[South target state]\n";
    const Model expected{"TB",
                         {{"A", "North source state", Shape::Rectangle},
                          {"B", "South target state", Shape::Rectangle}},
                         {{0, 1, "", "solid", false, true}},
                         {}};
    auto element = loom::ui::render_markdown(fenced(directed, "mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(directed, "text"));
    element->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height =
        std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    std::array<std::optional<Grid>, 5> frames;
    OracleWork work{kFlow400SceneLimit};
    for (std::size_t n = 0; n < frames.size(); ++n)
        frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
    for (const auto& frame : frames)
        static_cast<void>(flow400_following(*frame, work, kSentinel));
    EXPECT_FALSE(flow400_same(*frames[0], *frames[3], work));
    EXPECT_TRUE(flow400_same(*frames[0], *frames[2], work));
    EXPECT_TRUE(flow400_geometry(*frames[0], expected, work, kSentinel).empty());
    if (flow400_same(*frames[1], *frames[4], work))
        SUCCEED() << "Narrow rendering uses exact ordinary fallback";
    else
        EXPECT_TRUE(flow400_geometry(*frames[1], expected, work, kSentinel).empty());
    for (const auto& frame : frames)
        EXPECT_NE(flow400_following(*frame, work, kSentinel), frame->height);
    check_source_policy("flowchart\nsubgraph G\nA --> B\nend\n");
}

TEST(MermaidFlowResources, RetainsGroupCountAndDepthBounds) {
    auto nested = [](int depth) {
        std::string source = "flowchart TD\n";
        for (int n = 0; n < depth; ++n)
            source += "subgraph G" + std::to_string(n) + "\n";
        for (int n = 0; n < depth; ++n)
            source += "end\n";
        return source;
    };
    // Sixteen nested empty groups fit the independent cell limits. Seventeen
    // must be rejected by the depth policy, not rendered with flattened groups.
    Model sixteen_groups{"TD", {}, {}, {}};
    for (int n = 0; n < 16; ++n)
        sixteen_groups.groups.push_back(
            {"G" + std::to_string(n), "G" + std::to_string(n), "", n - 1, {}});
    check_source_policy(nested(16), true, &sixteen_groups);
    check_source_policy(nested(17));
    auto siblings = [](int count) {
        std::string source = "flowchart TD\n";
        for (int n = 0; n < count; ++n)
            source += "subgraph G" + std::to_string(n) + "\nend\n";
        return source;
    };
    // Sixty-four disconnected groups already exceed the canvas width; sixty-
    // five also exceed the group count. Both retain all source, never a prefix.
    check_source_policy(siblings(64));
    check_source_policy(siblings(65));
}

TEST(MermaidFlowResources, CountsMultilineLabelsAsOneAggregateBudget) {
    // The line separator consumes one of the 256 retained label units. Raw
    // <br> syntax is longer, so this also distinguishes encoded source bytes.
    const std::string at_limit =
        "flowchart TD\nA[" + std::string(128, 'x') + "<br>" + std::string(127, 'y') + "]\n";
    const std::string over_limit =
        "flowchart TD\nA[" + std::string(128, 'x') + "<br>" + std::string(128, 'y') + "]\n";
    const Model label_model{
        "TD", {{"A", std::string(128, 'x') + "\n" + std::string(127, 'y')}}, {}, {}};
    check_source_policy(at_limit, true, &label_model);
    check_source_policy(over_limit);
}

TEST(MermaidFlowResources, PreflightsCartesianEdgesAndNodeCounts) {
    auto cartesian = [](int right_count) {
        std::string source = "flowchart TD\n";
        // Thirty-two disconnected roots plus the sixteen left endpoints exceed
        // 240 columns even before spacing, while the total stays below 64.
        for (int n = 0; n < 32; ++n)
            source += "Detached" + std::to_string(n) + '\n';
        for (int n = 0; n < 16; ++n)
            source += (n ? " & A" : "A") + std::to_string(n);
        source += " --> ";
        for (int n = 0; n < right_count; ++n)
            source += (n ? " & B" : "B") + std::to_string(n);
        return source + '\n';
    };
    // All nodes are below 64. The first product is exactly 128 edges but cannot
    // fit this canvas; 16x9 must reject 144 edges before accepting a partial DAG.
    check_source_policy(cartesian(8));
    check_source_policy(cartesian(9));
    auto nodes = [](int count) {
        std::string source = "flowchart TD\n";
        for (int n = 0; n < count; ++n)
            source += "N" + std::to_string(n) + '\n';
        return source;
    };
    check_source_policy(nodes(64));
    check_source_policy(nodes(65));
}

TEST(MermaidFlowResources, RejectsMalformedForwardAndAggregateGroupEndpointGrammarWhole) {
    const std::array<std::string_view, 7> sources{
        {"flowchart TD\nsubgraph G\nA --> B\n", "flowchart TD\nend\nA --> B\n",
         "flowchart TD\nA --> G\nsubgraph G\nB\nend\n", "flowchart TD\nsubgraph G [\nA\nend\n",
         "flowchart TD\nsubgraph G\nA\nend\nA & G --> B\n",
         "flowchart TD\nsubgraph G\nA\nend\nG & A --> B\n",
         "flowchart TD\nsubgraph G\nA\nend\nG --> A & B\n"}};
    for (const auto source : sources) {
        SCOPED_TRACE(std::string(source));
        check_source_policy(source);
    }
}
}  // namespace

namespace {
// Source-only authority approved independently; captured cells never select Models.
std::size_t flow400_payload(const Grid& grid, OracleWork& work) {
    work.product(grid.cells.size(), 8u);
    std::size_t bytes = 0;
    for (const auto& cell : grid.cells) {
        flow400_require(cell.size() <= std::numeric_limits<std::size_t>::max() - bytes,
                        "Flow400 resource payload overflow");
        bytes += cell.size();
    }
    return bytes;
}

void flow400_publication(const Grid& grid, OracleWork& work) {
    const auto T = flow400_payload(grid, work), H = static_cast<std::size_t>(grid.height);
    work.product(T, 48u);
    work.product(3u * H + 32u, 8u);
    work.product(H, 3u * sizeof(std::string) + 4u * sizeof(json::JsonMutVal) + 64u);
    work.spend(4u * sizeof(json::JsonDoc) + 512u);
}

void flow400_append_frame(json::JsonMutVal row, std::string_view key, const Grid& grid) {
    // Its full payload/JSON escaping/serialization/write shares were prepaid.
    auto lines = row.make_arr();
    for (int y = 0; y < grid.height; ++y)
        lines.append(row.make_str(grid.row(y)));
    row.add(key, lines);
}

struct Flow400Observation {
    std::array<std::optional<Grid>, 5> frames;
    std::array<std::size_t, 8> remaining{};
    std::array<char, 257> error{};
    std::size_t captures = 0, published = 0;
    bool resource = false, model_decoded = false, whole = false, positive_unmet = false;
};

// Source4's case-specific catalogue remains local to the Native full-400
// consumer. Generic clone/caption/frame checks are test-private header helpers.
std::size_t flow400_typed_source4_mutations(const Grid& good, const Model& model,
                                            JsonVal source4_case, OracleWork& work) {
    const auto baseline = flow400_geometry(good, model, work);
    flow400_require(baseline.empty(), "Flow400 TypedSource4 mutation baseline");
    const auto id = flow400_text(source4_case.get("case_id"));
    const auto expected = flow400_typed_source4_expected(source4_case);
    std::size_t rejected = flow400_mutations(good, model, work);
    flow400_require(rejected == 3u, "Flow400 TypedSource4 exact legacy mutation count");
    work.spend(256u * sizeof(ShapeFrame) + 8192u);
    const auto contours = closed_frames(good, work);
    const auto node_index = [&](std::string_view node_id) {
        const auto node = std::ranges::find_if(
            model.nodes, [&](const Node& value) { return value.id == node_id; });
        flow400_require(node != model.nodes.end(), "Flow400 TypedSource4 selected node absent");
        return static_cast<std::size_t>(node - model.nodes.begin());
    };
    const auto frame = [&](std::size_t index) {
        const auto found = flow_typed_frame_for_node(good, contours, model.nodes[index], work);
        flow400_require(found.has_value(), "Flow400 TypedSource4 unique selected contour");
        return *found;
    };
    const auto semantic = [&](auto&& mutate, std::string_view name) {
        flow_typed_expect_model_rejected(good, model, work, mutate, name);
        ++rejected;
    };
    const auto physical = [&](auto&& mutate, std::string_view name) {
        flow_typed_expect_rejected(good, model, work, mutate, name);
        ++rejected;
    };
    if (id == "original_000") {
        const auto b = node_index("B"), e = node_index("E");
        semantic([&](Model& bad) { bad.nodes[b].shape = Shape::Rectangle; },
                 "Source4 original000 B diamond identity");
        semantic([&](Model& bad) { bad.nodes[e].shape = Shape::Rectangle; },
                 "Source4 original000 E diamond identity");
        flow400_require(model.edges.size() == 6u && model.edges[1].label == "Yes" &&
                            model.edges[2].label == "No",
                        "Flow400 TypedSource4 original000 label order");
        semantic([](Model& bad) { std::swap(bad.edges[1].label, bad.edges[2].label); },
                 "Source4 original000 Yes/No semantic swap");
        const auto yes = flow_typed_caption_blocks(good, "Yes", work),
                   no = flow_typed_caption_blocks(good, "No", work);
        flow400_require(yes.size() == 2u && no.size() == 2u,
                        "Flow400 TypedSource4 original000 repeated labels");
        physical([&](Grid& bad) { flow_typed_erase_label(bad, yes.front(), "Yes", work); },
                 "Source4 original000 erase Yes");
        const auto orphan = flow_typed_orphan_label_start(good, "No", contours, work);
        flow400_require(orphan.has_value(), "Flow400 TypedSource4 original000 No orphan witness");
        physical(
            [&](Grid& bad) {
                flow_typed_erase_label(bad, no.back(), "No", work);
                flow_typed_write_at(bad, *orphan, "No", work);
                flow400_require(flow_typed_caption_blocks(bad, "No", work).size() == 2u,
                                "Flow400 TypedSource4 original000 No count preserved");
            },
            "Source4 original000 No off owner path");
    } else if (id == "original_282") {
        const auto b = node_index("B"), c = node_index("C");
        const auto rounded = frame(b), decision = frame(c);
        semantic([&](Model& bad) { bad.nodes[b].shape = Shape::Rectangle; },
                 "Source4 original282 B rounded identity");
        physical([&](Grid& bad) { bad.put({rounded.left, rounded.top}, "┌"); },
                 "Source4 original282 B mixed rounded corner");
        semantic([&](Model& bad) { bad.nodes[c].shape = Shape::Rectangle; },
                 "Source4 original282 C diamond identity");
        const Point shoulder{decision.left + 1, decision.top + 1};
        flow400_require(good.at(shoulder) == "╱",
                        "Flow400 TypedSource4 original282 C diagonal witness");
        physical([&](Grid& bad) { bad.put(shoulder, " "); },
                 "Source4 original282 C missing diagonal");
        semantic(
            [&](Model& bad) {
                const auto at = bad.nodes[c].label.find('\n');
                flow400_require(at != std::string::npos,
                                "Flow400 TypedSource4 original282 multiline Model witness");
                bad.nodes[c].label.erase(at, 1u);
            },
            "Source4 original282 flattened C Model");
        const auto labels = flow_typed_caption_blocks(good, model.nodes[c].label, work);
        flow400_require(labels.size() == 1u && labels.front().starts.size() == 3u,
                        "Flow400 TypedSource4 original282 C three-line label witness");
        physical([&](Grid& bad) { bad.put(labels.front().starts[1], " "); },
                 "Source4 original282 C middle label glyph");
    } else if (id == "original_374") {
        const auto n1 = node_index("n1");
        const auto decision = frame(n1);
        semantic([&](Model& bad) { bad.nodes[n1].shape = Shape::Rectangle; },
                 "Source4 original374 diamond identity");
        const Point shoulder{decision.left + 1, decision.top + 1};
        flow400_require(good.at(shoulder) == "╱",
                        "Flow400 TypedSource4 original374 diagonal witness");
        physical([&](Grid& bad) { bad.put(shoulder, " "); },
                 "Source4 original374 missing diagonal");
    } else {
        flow400_require(id == "original_375", "Flow400 TypedSource4 unknown case mutation");
        const auto n1 = node_index("n1"), n3 = node_index("n3");
        const auto rounded = frame(n1);
        semantic([&](Model& bad) { bad.nodes[n1].shape = Shape::Rectangle; },
                 "Source4 original375 rounded identity");
        physical([&](Grid& bad) { bad.put({rounded.left, rounded.top}, "┌"); },
                 "Source4 original375 rounded mixed corner");
        semantic([&](Model& bad) { bad.nodes[n3].shape = Shape::Rounded; },
                 "Source4 original375 explicit rect identity");
        // n2 and n3 have the same (rectangle,label) signature. The Model fixes two
        // such source rectangles, but their frame assignment is intentionally symmetric.
        const auto rectangle_candidates =
            frame_candidates(good, contours, "Untitled Node", Shape::Rectangle, work);
        flow400_require(rectangle_candidates.size() == 2u,
                        "Flow400 TypedSource4 original375 two source rectangle contours");
        const auto& rectangle = contours[static_cast<std::size_t>(rectangle_candidates.front())];
        flow400_require(good.at({rectangle.left, rectangle.top}) == "┌",
                        "Flow400 TypedSource4 original375 rectangle corner witness");
        physical([&](Grid& bad) { bad.put({rectangle.left, rectangle.top}, "╭"); },
                 "Source4 original375 one required rectangle mixed corner");
    }
    flow400_require(rejected == expected, "Flow400 TypedSource4 exact source mutation count");
    return rejected;
}

// The Classic overlay deliberately uses only source-pinned Model fields. Every
// mutation starts from the real captured grid and must reject without resource
// exhaustion; no mutation derives a Model from renderer pixels.
std::size_t flow400_classic_mutations(const Grid& good, const Model& model, JsonVal classic_case,
                                      OracleWork& work) {
    const auto baseline = flow400_geometry(good, model, work);
    flow400_require(baseline.empty(), "Flow400 Classic mutation baseline");
    const auto expected = flow400_classic_expected(classic_case);
    const auto id = flow400_text(classic_case.get("case_id"));
    const auto node_index = [&](std::string_view node_id) {
        const auto found =
            std::ranges::find_if(model.nodes, [&](const Node& node) { return node.id == node_id; });
        flow400_require(found != model.nodes.end(), "Flow400 Classic selected node absent");
        return static_cast<std::size_t>(found - model.nodes.begin());
    };
    std::size_t rejected = 0u;
    const auto semantic = [&](auto&& mutate, std::string_view name) {
        flow_typed_expect_model_rejected(good, model, work, mutate, name);
        ++rejected;
    };
    const auto physical = [&](auto&& mutate, std::string_view name) {
        flow_typed_expect_rejected(good, model, work, mutate, name);
        ++rejected;
    };
    const auto extra_ink = [&](std::string_view name) {
        work.spend(4096u);
        std::optional<Point> blank;
        for (int y = 0; y < good.height && !blank; ++y)
            for (int x = 0; x < good.width; ++x) {
                work.spend(8u);
                if (good.at({x, y}) == " ") {
                    blank = Point{x, y};
                    break;
                }
            }
        flow400_require(blank.has_value(), "Flow400 Classic extra-ink blank witness");
        physical([&](Grid& bad) { bad.put(*blank, "§"); }, name);
    };
    if (id == "original_13_dagre_title_top_margin_zero") {
        const auto start = node_index("A");
        semantic([&](Model& bad) { bad.nodes[start].shape = Shape::Circle; },
                 "Classic13 titleTopMargin node shape");
        semantic([&](Model& bad) { bad.nodes[start].label = "A absent"; },
                 "Classic13 titleTopMargin node label");
        semantic([](Model& bad) { bad.edges.front().to = bad.edges.front().from; },
                 "Classic13 titleTopMargin edge endpoint");
        extra_ink("Classic13 titleTopMargin extra ink");
    } else if (id == "original_40_diamond_fanout") {
        const auto decision = node_index("A");
        semantic([&](Model& bad) { bad.nodes[decision].shape = Shape::Rectangle; },
                 "Classic40 diamond identity");
        semantic([&](Model& bad) { bad.nodes[decision].label = "A absent"; },
                 "Classic40 decision label");
        semantic([](Model& bad) { bad.edges[1].to = bad.edges[1].from; },
                 "Classic40 fanout target");
        extra_ink("Classic40 extra ink");
    } else if (id == "original_41_diamond_fanout_in_subgraph") {
        const auto decision = node_index("A");
        semantic([&](Model& bad) { bad.nodes[decision].shape = Shape::Rectangle; },
                 "Classic41 diamond identity");
        semantic([&](Model& bad) { bad.nodes[decision].label = "A absent"; },
                 "Classic41 decision label");
        semantic([](Model& bad) { bad.edges[1].to = bad.edges[1].from; },
                 "Classic41 fanout target");
        semantic(
            [](Model& bad) {
                flow400_require(!bad.groups.empty() && bad.groups.front().nodes.size() == 1u,
                                "Classic41 subbe membership witness");
                bad.groups.front().nodes.clear();
            },
            "Classic41 subbe membership");
        extra_ink("Classic41 extra ink");
    } else if (id == "original_244_dagre_subgraphs") {
        const auto database = node_index("D"), client = node_index("A");
        semantic([&](Model& bad) { bad.nodes[database].shape = Shape::Circle; },
                 "Classic244 cylinder-to-circle");
        semantic([&](Model& bad) { bad.nodes[client].label = "Client absent"; },
                 "Classic244 Client label");
        semantic([](Model& bad) { bad.edges.front().to = bad.edges.front().from; },
                 "Classic244 first edge endpoint");
        semantic(
            [](Model& bad) {
                flow400_require(!bad.groups.empty() && bad.groups.front().nodes.size() == 2u,
                                "Classic244 Frontend witness");
                bad.groups.front().nodes.erase(bad.groups.front().nodes.begin());
            },
            "Classic244 Frontend membership");
        extra_ink("Classic244 extra ink");
    } else if (id == "original_295_classic_trapezoids") {
        const auto christmas = node_index("A");
        semantic([&](Model& bad) { bad.nodes[christmas].shape = Shape::TrapezoidAlt; },
                 "Classic295 trapezoid identity");
        semantic([&](Model& bad) { bad.nodes[christmas].label = "Christmas absent"; },
                 "Classic295 Christmas label");
        semantic([](Model& bad) { bad.edges.front().to = bad.edges.front().from; },
                 "Classic295 first edge endpoint");
        extra_ink("Classic295 extra ink");
    } else {
        flow400_require(id == "original_376_stadium", "Flow400 Classic unknown mutation row");
        const auto start = node_index("A");
        semantic([&](Model& bad) { bad.nodes[start].shape = Shape::Circle; },
                 "Classic376 stadium identity");
        semantic([&](Model& bad) { bad.nodes[start].label = "Start absent"; },
                 "Classic376 Start label");
        semantic([](Model& bad) { bad.edges.front().to = bad.edges.front().from; },
                 "Classic376 first edge endpoint");
        extra_ink("Classic376 extra ink");
    }
    flow400_require(rejected == expected &&
                        expected == ((id == "original_244_dagre_subgraphs" ||
                                      id == "original_41_diamond_fanout_in_subgraph")
                                         ? 5u
                                         : 4u),
                    "Flow400 Classic mutation source catalogue");
    return rejected;
}

std::size_t flow400_linkprofile3_mutations(const Grid& good, const Model& model,
                                           const LinkProfile& profile, JsonVal link_case,
                                           OracleWork& work) {
    const LinkProfileMutationScene mutations(good, model, profile, work);
    const auto expected = flow400_linkprofile3_expected(link_case);
    const auto id = flow400_text(link_case.get("case_id"));
    std::size_t rejected = 0u;
    const auto physical = [&](auto&& mutate, std::string_view name) {
        mutations.physical(mutate, name);
        ++rejected;
    };
    const auto semantic = [&](auto&& mutate, std::string_view name) {
        mutations.model(mutate, name);
        ++rejected;
    };
    const auto paired_semantic = [&](auto&& mutate, std::string_view name) {
        // The expectation remains a valid closed Model/Profile. It must be
        // rejected by the actual captured Grid, never by profile schema drift.
        flow400_copy_model(model, work);
        flow400_linkprofile3_copy(profile, work);
        work.spend(2048u);
        auto bad_model = model;
        auto bad_profile = profile;
        mutate(bad_model, bad_profile);
        flow400_require(linkprofile3_profile_shape(bad_model, bad_profile),
                        "Flow400 LinkProfile3 paired semantic Model/Profile validity");
        const auto errors = flow_linkprofile3_geometry(good, bad_model, bad_profile, work);
        if (errors.empty())
            throw std::runtime_error(std::string(name) + " physical expectation admitted");
        for (const auto& error : errors)
            if (flow400_resource(error))
                throw std::runtime_error(std::string(name) + " became resource credit");
        ++rejected;
    };
    const auto arrow = [&]() {
        std::optional<Point> found;
        for (int y = 0; y < good.height && !found; ++y)
            for (int x = 0; x < good.width; ++x)
                if (glyph(good.at({x, y})).arrow >= 0) {
                    found = Point{x, y};
                    break;
                }
        flow400_require(found.has_value(), "Flow400 LinkProfile3 point-marker witness");
        return *found;
    };
    const auto contours = closed_frames(good, work);
    const auto extra_ink = [&](std::string_view name) {
        const auto footer = flow400_following(good, work);
        std::optional<Point> blank;
        for (int y = 0; y < footer && !blank; ++y)
            for (int x = 0; x < good.width; ++x) {
                work.spend();
                const Point point{x, y};
                if (good.at(point) == " ") {
                    blank = point;
                    break;
                }
            }
        flow400_require(blank.has_value(), "Flow400 LinkProfile3 extra-ink witness");
        physical([&](Grid& bad) { bad.put(*blank, "§"); }, name);
    };
    if (id == "link_profile_019" || id == "link_profile_291" || id == "link_profile_294") {
        const std::size_t marker_edge = 4u, minimum_edge = 7u, label_edge = 5u;
        flow400_require(profile.edges.size() > minimum_edge && model.edges.size() > label_edge,
                        "Flow400 LinkProfile3 source edge catalogue");
        const int minimum_target = model.edges[minimum_edge].to;
        flow400_require(
            minimum_target >= 0 && static_cast<std::size_t>(minimum_target) < model.nodes.size() &&
                std::ranges::none_of(model.edges,
                                     [&](const Edge& edge) { return edge.from == minimum_target; }),
            "Flow400 LinkProfile3 minimum target leaf witness");
        paired_semantic(
            [&](Model& bad_model, LinkProfile& bad_profile) {
                bad_model.edges[marker_edge].start = false;
                bad_profile.edges[marker_edge].source = LinkMarker::None;
            },
            "LinkProfile3 point-start wrong physical expectation");
        paired_semantic(
            [&](Model&, LinkProfile& bad_profile) {
                ++bad_profile.edges[minimum_edge].minimum_length;
                ++bad_profile.node_ranks[static_cast<std::size_t>(minimum_target)];
            },
            "LinkProfile3 source minimum-length wrong physical expectation");
        semantic([&](Model& bad) { bad.edges[label_edge].label = "Label 1 absent"; },
                 "LinkProfile3 Label 1 semantic text");
        const auto marker = arrow();
        physical(
            [&](Grid& bad) {
                const auto direction = glyph(bad.at(marker)).arrow;
                bad.put(marker, (direction == 0 || direction == 2) ? "│" : "─");
            },
            "LinkProfile3 point marker removed");
        const auto labels = flow_typed_caption_blocks(good, "Label 2", work);
        flow400_require(labels.size() == 1u, "Flow400 LinkProfile3 Label 2 physical block");
        const auto orphan = flow_typed_orphan_label_start(good, "Label 2", contours, work);
        flow400_require(orphan.has_value(), "Flow400 LinkProfile3 Label 2 orphan witness");
        physical(
            [&](Grid& bad) {
                flow_typed_erase_label(bad, labels.front(), "Label 2", work);
                flow_typed_write_at(bad, *orphan, "Label 2", work);
            },
            "LinkProfile3 Label 2 off-route caption");
        extra_ink("LinkProfile3 extra ink");
    } else {
        flow400_require(id == "link_profile_298" && model.edges.size() == 4u &&
                            profile.edges.size() == 4u,
                        "Flow400 LinkProfile3 open-edge catalogue");
        const int minimum_target = model.edges[0u].to;
        flow400_require(
            minimum_target >= 0 && static_cast<std::size_t>(minimum_target) < model.nodes.size() &&
                std::ranges::none_of(model.edges,
                                     [&](const Edge& edge) { return edge.from == minimum_target; }),
            "Flow400 LinkProfile3 open minimum target leaf witness");
        semantic([](Model& bad) { bad.edges[1u].label = "invented"; },
                 "LinkProfile3 C-to-D invented label");
        paired_semantic(
            [&](Model&, LinkProfile& bad_profile) {
                ++bad_profile.edges[0u].minimum_length;
                ++bad_profile.node_ranks[static_cast<std::size_t>(minimum_target)];
            },
            "LinkProfile3 open minimum-length wrong physical expectation");
        const auto marker = arrow();
        physical(
            [&](Grid& bad) {
                const auto direction = glyph(bad.at(marker)).arrow;
                bad.put(marker, (direction == 0 || direction == 2) ? "│" : "─");
            },
            "LinkProfile3 bidirectional marker removed");
        const auto node =
            std::ranges::find_if(model.nodes, [](const Node& value) { return value.id == "A"; });
        flow400_require(node != model.nodes.end(), "Flow400 LinkProfile3 A node");
        const auto source_frame = flow_typed_frame_for_node(good, contours, *node, work);
        flow400_require(source_frame.has_value(), "Flow400 LinkProfile3 A contour");
        const Point port{(source_frame->left + source_frame->right + 1) / 2, source_frame->bottom};
        flow400_require(good.at(port) == "┬",
                        "Flow400 LinkProfile3 open source contour-arm witness");
        physical([&](Grid& bad) { bad.put(port, "│"); }, "LinkProfile3 open source contour arms");
        const auto labels = flow_typed_caption_blocks(good, "bidirectional", work);
        flow400_require(labels.size() == 1u, "Flow400 LinkProfile3 bidirectional caption");
        const auto orphan = flow_typed_orphan_label_start(good, "bidirectional", contours, work);
        flow400_require(orphan.has_value(), "Flow400 LinkProfile3 spare caption witness");
        physical([&](Grid& bad) { flow_typed_write_at(bad, *orphan, "bidirectional", work); },
                 "LinkProfile3 spare caption");
        extra_ink("LinkProfile3 extra ink");
    }
    flow400_require(rejected == expected && expected == 6u,
                    "Flow400 LinkProfile3 source mutation catalogue");
    return rejected;
}

namespace {
TEST(MermaidFlowTypedSource4Authority, RejectsForgedRowsAndBindsOnlyFourBaseNullOrdinals) {
    auto originals = flow400_documents();
    auto config21 = flow400_config21_documents();
    auto typed = flow400_typed_source4_documents();
    const auto root = typed.supplement.root(), case282 = root.get("cases").at(1u),
               authority = case282.get("base_authority");
    const auto ordinal = flow400_number(authority.get("ordinal")),
               global = flow400_number(authority.get("manifest_global_ordinal"));
    const auto fixture = originals.manifest.root().get("fixtures").at(global);
    const auto path = flow400_text(fixture.get("path"));
    const auto source = flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                                     "fixtures/mermaid_official/cases" / std::string(path));
    OracleWork work{kFlow400SceneLimit};
    const auto selected = flow400_typed_source4_bound_authority(
        originals.compact.root().get("cases").at(ordinal), config21.supplement.root(), root,
        fixture, ordinal, global, source, work);
    EXPECT_EQ(selected.kind, Flow400TypedSource4Kind::TypedSource4);
    EXPECT_EQ(selected.exact_rejections, 9u);
    const auto model = flow400_typed_source4_model(selected, work);
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->nodes.size(), 6u);
    EXPECT_EQ(model->edges.size(), 5u);
    bool bad_source = false;
    try {
        static_cast<void>(flow400_typed_source4_bound_authority(
            originals.compact.root().get("cases").at(ordinal), config21.supplement.root(), root,
            fixture, ordinal, global, source + "X", work));
    } catch (const std::exception& error) {
        bad_source = error.what() == std::string_view("Flow400 protocol complete source bytes");
    }
    EXPECT_TRUE(bad_source);
    const auto text = root.to_string();
    const auto model_sha = flow400_text(case282.get("typed_model_sha256"));
    auto wrong_sha = text;
    const auto model_at = wrong_sha.find(model_sha);
    ASSERT_NE(model_at, std::string::npos);
    wrong_sha.replace(model_at, model_sha.size(), std::string(64u, '0'));
    auto parsed_sha = json::parse(wrong_sha);
    ASSERT_TRUE(parsed_sha.has_value());
    OracleWork sha_work{kFlow400SceneLimit};
    EXPECT_THROW(flow400_typed_source4_root(parsed_sha->root(), sha_work), std::runtime_error);
    auto unknown = text;
    const auto status = unknown.find("\"formal_status\"");
    ASSERT_NE(status, std::string::npos);
    unknown.replace(status, std::string_view("\"formal_status\"").size(), "\"unknown_status\"");
    auto parsed_unknown = json::parse(unknown);
    ASSERT_TRUE(parsed_unknown.has_value());
    OracleWork unknown_work{kFlow400SceneLimit};
    EXPECT_THROW(flow400_typed_source4_root(parsed_unknown->root(), unknown_work),
                 std::runtime_error);
    auto wrong_plan = text;
    const auto legacy = wrong_plan.find("\"legacy_three\"");
    ASSERT_NE(legacy, std::string::npos);
    wrong_plan.replace(legacy, std::string_view("\"legacy_three\"").size(), "\"legacy_two\"");
    auto parsed_wrong_plan = json::parse(wrong_plan);
    ASSERT_TRUE(parsed_wrong_plan.has_value());
    OracleWork wrong_plan_work{kFlow400SceneLimit};
    bool wrong_plan_rejected = false;
    try {
        flow400_typed_source4_root(parsed_wrong_plan->root(), wrong_plan_work);
    } catch (const std::exception& error) {
        wrong_plan_rejected =
            error.what() == std::string_view("Flow400 TypedSource4 native mutation plan");
    }
    EXPECT_TRUE(wrong_plan_rejected);
    auto wrong_case_id = text;
    const auto original282 = wrong_case_id.find("\"case_id\":\"original_282\"");
    ASSERT_NE(original282, std::string::npos);
    wrong_case_id.replace(original282, std::string_view("\"case_id\":\"original_282\"").size(),
                          "\"case_id\":\"original_000\"");
    auto parsed_wrong_case_id = json::parse(wrong_case_id);
    ASSERT_TRUE(parsed_wrong_case_id.has_value());
    const auto exact_tuple_rejected = [&](auto&& action) {
        bool rejected = false;
        try {
            action();
        } catch (const std::exception& error) {
            rejected = error.what() ==
                       std::string_view("Flow400 TypedSource4 case-id/ordinal/mutation tuple");
        }
        EXPECT_TRUE(rejected);
    };
    OracleWork wrong_case_id_work{kFlow400SceneLimit};
    exact_tuple_rejected(
        [&] { flow400_typed_source4_root(parsed_wrong_case_id->root(), wrong_case_id_work); });
    OracleWork selector_wrong_case_id_work{kFlow400SceneLimit};
    exact_tuple_rejected([&] {
        static_cast<void>(flow400_typed_source4_bound_authority(
            originals.compact.root().get("cases").at(ordinal), config21.supplement.root(),
            parsed_wrong_case_id->root(), fixture, ordinal, global, source,
            selector_wrong_case_id_work));
    });
    auto duplicate = text;
    const auto second = duplicate.find("\"ordinal\":282");
    ASSERT_NE(second, std::string::npos);
    duplicate.replace(second, std::string_view("\"ordinal\":282").size(), "\"ordinal\":0");
    auto parsed_duplicate = json::parse(duplicate);
    ASSERT_TRUE(parsed_duplicate.has_value());
    OracleWork duplicate_work{kFlow400SceneLimit};
    EXPECT_THROW(flow400_typed_source4_root(parsed_duplicate->root(), duplicate_work),
                 std::runtime_error);
    OracleWork zero{0u};
    EXPECT_THROW(flow400_typed_source4_case_shape(case282, zero), std::runtime_error);
}
}  // namespace

TEST(MermaidFlowSemantics, Official400SourceAuthorityAndStrictFollowingProtocol) {
    // Bounded borrowed diagnostics: resource/protocol failures are distinct obligations.
    const auto expect_failure = [](auto&& action, std::string_view expected,
                                   bool resource_expected) {
        bool rejected = false;
        try {
            action();
        } catch (const std::exception& error) {
            const auto* raw = error.what();
            std::size_t size = 0;
            if (raw)
                while (size < 256u && raw[size])
                    ++size;
            const auto message = std::string_view(raw ? raw : "", size);
            const bool exact_kind =
                resource_expected
                    ? message == "Bounded cell-oracle work exhausted" && flow400_resource(message)
                    : message.starts_with("Flow400 protocol ") && !flow400_resource(message);
            rejected = exact_kind && message == expected;
        }
        EXPECT_TRUE(rejected);
    };
    auto docs = flow400_documents();
    OracleWork integrity;
    const auto fixtures = docs.manifest.root().get("fixtures"),
               cases = docs.compact.root().get("cases");
    integrity.spend(4u * sizeof(JsonVal) + 8u * sizeof(std::size_t) + 256u);
    JsonVal paired_row, paired_fixture;
    std::size_t paired_global = 0, nonnull = 0, pending = 0, visited = 0, global_ordinal = 0;
    fixtures.iter([&](JsonVal fixture) {
        integrity.spend(128u);
        const auto global = global_ordinal++;
        const auto path = flow400_text(fixture.get("path"));
        integrity.spend(path.size());
        if (!path.starts_with("flowchart/")) return;
        integrity.spend(2048u + path.size() * 4u);
        flow400_require(visited < 400u, "Flow400 protocol extra integrity source");
        const auto i = visited++;
        const auto row = cases.at(i);
        if (i == 1u) {
            paired_row = row;
            paired_fixture = fixture;
            paired_global = global;
        }
        integrity.product(flow400_number(fixture.get("bytes")), 4u);
        const auto source = flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                                         "fixtures/mermaid_official/cases" / std::string(path));
        flow400_bind(row, fixture, i, global, source, integrity);
        if (flow400_model(row, integrity))
            ++nonnull;
        else
            ++pending;
    });
    EXPECT_EQ(global_ordinal, 2145u);
    EXPECT_EQ(visited, 400u);
    EXPECT_EQ(nonnull, 8u);
    EXPECT_EQ(pending, 392u);
    // Source-bound parsed controls: no schema/resource result is geometry credit.
    {
        integrity.spend(8192u);
        ASSERT_TRUE(paired_row.valid() && paired_fixture.valid());
        const auto row = paired_row, fixture = paired_fixture;
        const auto global = paired_global;
        const auto path = flow400_text(fixture.get("path"));
        integrity.product(flow400_number(fixture.get("bytes")), 4u);
        const auto source = flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                                         "fixtures/mermaid_official/cases" / std::string(path));
        ASSERT_NO_THROW(flow400_bind(row, fixture, 1u, global, source, integrity));
        expect_failure([&] { flow400_bind(row, fixture, 2u, global, source, integrity); },
                       "Flow400 protocol source ordinal", false);
        expect_failure([&] { flow400_bind(row, fixture, 1u, global + 1u, source, integrity); },
                       "Flow400 protocol source ordinal", false);
        auto substituted = source;
        substituted.push_back('X');
        expect_failure([&] { flow400_bind(row, fixture, 1u, global, substituted, integrity); },
                       "Flow400 protocol complete source bytes", false);
        auto extra = row.to_string();
        extra.insert(extra.size() - 1u, ",\"unknown\":true");
        auto bad = json::parse(extra);
        ASSERT_TRUE(bad.has_value());
        expect_failure([&] { flow400_bind(bad->root(), fixture, 1u, global, source, integrity); },
                       "Flow400 protocol object keys", false);
        auto text = row.to_string();
        const auto hash = flow400_text(row.get("model_sha256"));
        const auto pos = text.find(hash);
        ASSERT_NE(pos, std::string::npos);
        text.replace(pos, hash.size(), std::string(64u, '0'));
        auto wrong = json::parse(text);
        ASSERT_TRUE(wrong.has_value());
        expect_failure([&] { flow400_model(wrong->root(), integrity); },
                       "Flow400 protocol Model text SHA", false);
    }
    OracleWork work{kFlow400SceneLimit};
    work.spend(32768u);
    constexpr std::string_view pair_source = "flowchart LR\na[A] --> b[B]\n";
    ASSERT_EQ(digest(pair_source),
              "436c29b87458686a53a8c1e750f947c83edb1fda07bc6ea5876bddfe27dd77b4");
    const Model model{"LR", {{"a", "A"}, {"b", "B"}}, {{0, 1, "", "solid", false, true}}, {}};
    const auto good = mock({"┌───┐     ┌───┐         ", "│ A ├────▶│ B │         ",
                            "└───┘     └───┘         ", "", kSentinel, ""});
    ASSERT_TRUE(flow400_geometry(good, model, work).empty());
    for (int kind = 0; kind < 5; ++kind) {
        ASSERT_TRUE(flow400_geometry(good, model, work).empty());
        flow400_copy_grid(good, work);
        auto bad = good;
        work.spend(1024u);
        if (kind == 0)
            for (std::size_t x = 0; x < kSentinel.size(); ++x)
                bad.put({static_cast<int>(x), 4}, " ");
        if (kind == 1)
            for (std::size_t x = 0; x < kSentinel.size(); ++x)
                bad.put({static_cast<int>(x), 3}, std::string(kSentinel.substr(x, 1u)));
        if (kind == 2) bad.put({20, 4}, "X");
        if (kind == 3) bad.put({0, 5}, "X");
        if (kind == 4) {
            for (std::size_t x = 0; x < kSentinel.size(); ++x)
                bad.put({static_cast<int>(x), 4}, " ");
            bad.put({0, 4}, std::string(kSentinel));
        }
        expect_failure([&] { flow400_following(bad, work); },
                       "Flow400 protocol exact independent following row", false);
    }
    flow400_copy_grid(good, work);
    auto restored = good;
    work.spend(32u);
    restored.put({2, 1}, "Z");
    EXPECT_FALSE(flow400_same(good, restored, work));
    flow400_copy_grid(good, work);
    auto dimensions = good;
    --dimensions.height;
    EXPECT_FALSE(flow400_same(good, dimensions, work));
    work.spend(32768u);
    constexpr std::string_view collision_source = "flowchart LR\nsame[AFTER_FLOW_SEMANTICS]\n";
    ASSERT_EQ(digest(collision_source),
              "0f89d3435c5e8f1bef56768d7c3a2400a7f309b2bbe79ef080bcb2a285f7398d");
    const Model collision{"LR", {{"same", std::string(kSentinel)}}, {}, {}};
    const auto named = mock({"┌──────────────────────┐", "│ AFTER_FLOW_SEMANTICS │",
                             "└──────────────────────┘", "", kSentinel});
    ASSERT_TRUE(flow400_geometry(named, collision, work).empty());
    flow400_copy_grid(named, work);
    auto missing = named;
    work.spend(32u);
    missing.put({2, 1}, "X");
    EXPECT_FALSE(flow400_geometry(missing, collision, work).empty());
    work.spend(8192u);
    auto preexisting = ftxui::text("x");
    OracleWork zero{0u};
    expect_failure([&] { capture(preexisting, 1, 1, &zero); }, "Bounded cell-oracle work exhausted",
                   true);
    OracleWork zero_geometry{0u};
    expect_failure([&] { flow400_geometry(good, model, zero_geometry); },
                   "Bounded cell-oracle work exhausted", true);
}

TEST(MermaidFlowSemantics, OfficialIssue4648ArrowlessNestedSubgraphIsSourceBoundGeometry) {
    auto docs = flow400_documents();
    OracleWork work{kFlow400SceneLimit};
    work.spend(32768u);
    const auto source =
        flow400_read(std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" /
                     std::string(kFlow400Issue4648Fixture));
    ASSERT_EQ(source.size(), 529u);
    auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
    element->ComputeRequirement();
    ordinary->ComputeRequirement();
    const auto line_floor = static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 12;
    const int height = std::max(
        {32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8, line_floor});
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    std::array<Grid, 5> frames;
    for (std::size_t index = 0u; index < frames.size(); ++index)
        frames[index] = capture(index < 3u ? element : ordinary, widths[index], height, &work);
    // All five observations exist before the exact source tuple or semantic
    // Model is bound, so source fallback cannot stand in for a drawing.
    flow400_issue4648_bind(docs, source, work);
    const auto model = flow400_issue4648_model(work);
    EXPECT_FALSE(flow400_same(frames[0], frames[3], work));
    const bool restored = flow400_same(frames[0], frames[2], work);
    ASSERT_TRUE(restored);
    const auto wide = flow400_geometry(frames[0], model, work);
    ASSERT_TRUE(wide.empty()) << errors_text(wide);
    const bool narrow_source = flow400_same(frames[1], frames[4], work);
    bool narrow_geometry = false;
    if (narrow_source)
        EXPECT_NO_THROW(static_cast<void>(flow400_following(frames[1], work)));
    else {
        const auto narrow = flow400_geometry(frames[1], model, work);
        narrow_geometry = narrow.empty();
        EXPECT_TRUE(narrow_geometry) << errors_text(narrow);
    }

    auto wrong_endpoint = model;
    wrong_endpoint.edges[2].to = 1;
    const auto wrong_endpoint_errors = flow400_geometry(frames[0], wrong_endpoint, work);
    EXPECT_FALSE(wrong_endpoint_errors.empty());
    auto wrong_marker = model;
    wrong_marker.edges[2].end = true;
    const auto wrong_marker_errors = flow400_geometry(frames[0], wrong_marker, work);
    EXPECT_FALSE(wrong_marker_errors.empty());
    auto wrong_group = model;
    wrong_group.groups[0].direction = "TD";
    const auto wrong_group_errors = flow400_geometry(frames[0], wrong_group, work);
    EXPECT_FALSE(wrong_group_errors.empty());
    auto missing_edge = model;
    missing_edge.edges.pop_back();
    const auto missing_edge_errors = flow400_geometry(frames[0], missing_edge, work);
    EXPECT_FALSE(missing_edge_errors.empty());
    ASSERT_EQ(frames[0].at({35, 28}), "│");
    flow400_copy_grid(frames[0], work);
    auto broken_route = frames[0];
    broken_route.put({35, 28}, " ");
    const auto broken_route_errors = flow400_geometry(broken_route, model, work);
    EXPECT_FALSE(broken_route_errors.empty());
    flow400_copy_grid(frames[0], work);
    auto branched_route = frames[0];
    branched_route.put({35, 28}, "┼");
    const auto branched_route_errors = flow400_geometry(branched_route, model, work);
    EXPECT_FALSE(branched_route_errors.empty());
    flow400_copy_grid(frames[0], work);
    auto unexpected_arrow = frames[0];
    unexpected_arrow.put({35, 28}, "▼");
    const auto unexpected_arrow_errors = flow400_geometry(unexpected_arrow, model, work);
    EXPECT_FALSE(unexpected_arrow_errors.empty());
    flow400_copy_grid(frames[0], work);
    auto orphan_route = frames[0];
    orphan_route.put({100, 10}, "│");
    const auto orphan_route_errors = flow400_geometry(orphan_route, model, work);
    EXPECT_FALSE(orphan_route_errors.empty());
    flow400_copy_grid(frames[0], work);
    auto multiple_orphans = frames[0];
    multiple_orphans.put({100, 10}, "│");
    multiple_orphans.put({102, 10}, "│");
    const auto orphan_errors = flow400_geometry(multiple_orphans, model, work);
    ASSERT_FALSE(orphan_errors.empty());
    EXPECT_LE(orphan_errors.size(), 1u);
    for (const auto& error : orphan_errors)
        EXPECT_FALSE(flow400_resource(error));

    std::size_t profile_rejections = 0u;
    auto reject_profile = [&](std::string changed) {
        work.product(changed.size(), 4u);
        work.spend(1024u);
        auto invalid = json::parse(changed);
        ASSERT_TRUE(invalid.has_value());
        std::size_t groups = 0u, members = 0u;
        bool rejected = false;
        try {
            flow400_shape(invalid->root(), groups, members);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        EXPECT_TRUE(rejected);
        profile_rejections += rejected;
    };
    const auto edge_at = kFlow400Issue4648ModelJson.find("\"source\":\"D3\"");
    ASSERT_NE(edge_at, std::string_view::npos);
    auto labeled = std::string(kFlow400Issue4648ModelJson);
    const auto label_at = labeled.find("\"label\":\"\"", edge_at);
    ASSERT_NE(label_at, std::string::npos);
    labeled.replace(label_at, std::string_view("\"label\":\"\"").size(), "\"label\":\"label\"");
    reject_profile(std::move(labeled));
    for (const auto style : {std::string_view{"dotted"}, std::string_view{"thick"}}) {
        auto styled = std::string(kFlow400Issue4648ModelJson);
        const auto style_at = styled.find("\"style\":\"solid\"", edge_at);
        ASSERT_NE(style_at, std::string::npos);
        styled.replace(style_at, std::string_view("\"style\":\"solid\"").size(),
                       "\"style\":\"" + std::string(style) + "\"");
        reject_profile(std::move(styled));
    }
    auto reverse = std::string(kFlow400Issue4648ModelJson);
    const auto start_at = reverse.find("\"hasArrowStart\":false", edge_at);
    ASSERT_NE(start_at, std::string::npos);
    reverse.replace(start_at, std::string_view("\"hasArrowStart\":false").size(),
                    "\"hasArrowStart\":true");
    reject_profile(std::move(reverse));
    const auto physical_rejections = std::array{
        !broken_route_errors.empty(), !branched_route_errors.empty(),
        !unexpected_arrow_errors.empty(), !orphan_route_errors.empty(), !orphan_errors.empty()};
    const auto generic_mutations = flow400_mutations(frames[0], model, work);
    EXPECT_GT(generic_mutations, 0u);
    for (const auto* errors : {&wrong_endpoint_errors, &wrong_marker_errors, &wrong_group_errors,
                               &missing_edge_errors, &broken_route_errors, &branched_route_errors,
                               &unexpected_arrow_errors, &orphan_route_errors, &orphan_errors})
        for (const auto& error : *errors)
            EXPECT_FALSE(flow400_resource(error));
    const auto semantic_count = static_cast<std::size_t>(!wrong_endpoint_errors.empty()) +
                                static_cast<std::size_t>(!wrong_marker_errors.empty()) +
                                static_cast<std::size_t>(!wrong_group_errors.empty()) +
                                static_cast<std::size_t>(!missing_edge_errors.empty()) +
                                profile_rejections;
    const auto physical_count =
        static_cast<std::size_t>(std::ranges::count(physical_rejections, true));
    EXPECT_EQ(semantic_count, 8u);
    EXPECT_EQ(physical_count, 5u);
#ifdef LOOM_MERMAID_REPORT_DIR
    json::JsonMutDoc verdict;
    auto root = verdict.object();
    root.set("schema", "flow400_issue4648_native_verdict/v1");
    root.set("source_facts_sha256", kFlow400Issue4648FactsSha);
    root.set("case_id", "flow400_nested_external1_357");
    root.set("ordinal", static_cast<int64_t>(kFlow400Issue4648Ordinal));
    root.set("manifest_global_ordinal", static_cast<int64_t>(kFlow400Issue4648GlobalOrdinal));
    root.set("fixture", kFlow400Issue4648Fixture);
    root.set("source_sha256", kFlow400Issue4648SourceSha);
    root.set("model_sha256", kFlow400Issue4648ModelSha);
    root.set("captures", static_cast<int64_t>(frames.size()));
    root.set("captures_before_binding", true);
    root.set("source_bound_after_captures", true);
    root.set("wide_geometry_good", wide.empty());
    root.set("restored_geometry_good", restored);
    root.set("narrow_exact_ordinary", narrow_source);
    root.set("narrow_geometry_good", narrow_geometry);
    root.set("semantic_mutations_rejected", static_cast<int64_t>(semantic_count));
    root.set("physical_mutations_rejected", static_cast<int64_t>(physical_count));
    root.set("profile_controls_rejected", static_cast<int64_t>(profile_rejections));
    root.set("generic_mutations_rejected", static_cast<int64_t>(generic_mutations));
    root.set("resource_errors", 0);
    root.set("source_fallback_credit", 0);
    root.set("admitted", wide.empty() && restored && (narrow_source || narrow_geometry) &&
                             semantic_count == 8u && physical_count == 5u &&
                             profile_rejections == 4u && generic_mutations > 0u);
    verdict.set_root(root);
    const auto target = std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                        "mermaid-flow400-issue4648-native-verdict.json";
    std::ofstream output(target, std::ios::binary);
    ASSERT_TRUE(output && (output << verdict.to_string() << '\n')) << "Cannot publish " << target;
#endif
}

TEST(MermaidFlowClassicAuthority, BindsOnlySixFrozenBaseNullTuples) {
    auto docs = flow400_documents();
    auto config21 = flow400_config21_documents();
    auto typed = flow400_typed_source4_documents();
    auto classic = flow400_classic_documents();
    const auto cases = classic.supplement.root().get("cases");
    ASSERT_EQ(cases.size(), 6u);
    for (std::size_t index = 0u; index < cases.size(); ++index) {
        const auto row = cases.at(index);
        const auto ordinal = flow400_number(row.get("base_authority").get("ordinal"));
        const auto global =
            flow400_number(row.get("base_authority").get("manifest_global_ordinal"));
        const auto fixture = docs.manifest.root().get("fixtures").at(global);
        const auto source =
            flow400_read(std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" /
                         std::string(flow400_text(fixture.get("path"))));
        OracleWork work{kFlow400SceneLimit};
        const auto prior = flow400_typed_source4_bound_authority(
            docs.compact.root().get("cases").at(ordinal), config21.supplement.root(),
            typed.supplement.root(), fixture, ordinal, global, source, work);
        const auto selected = flow400_classic_select(prior, classic.supplement.root(), fixture,
                                                     ordinal, global, source, work);
        ASSERT_TRUE(selected.is_obj());
        EXPECT_EQ(flow400_text(selected.get("case_id")), flow400_text(row.get("case_id")));
        const auto model = flow400_classic_model(selected, work);
        EXPECT_FALSE(model.nodes.empty());
    }
    const auto ordinal = 295u, global = 956u;
    const auto fixture = docs.manifest.root().get("fixtures").at(global);
    const auto source =
        flow400_read(std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" /
                     std::string(flow400_text(fixture.get("path"))));
    OracleWork work{kFlow400SceneLimit};
    const auto prior = flow400_typed_source4_bound_authority(
        docs.compact.root().get("cases").at(ordinal), config21.supplement.root(),
        typed.supplement.root(), fixture, ordinal, global, source, work);
    EXPECT_THROW(static_cast<void>(flow400_classic_select(prior, classic.supplement.root(), fixture,
                                                          ordinal, global, source + "X", work)),
                 std::runtime_error);
    auto altered = classic.supplement.root().to_string();
    const auto at = altered.find("\"case_id\":\"original_295_classic_trapezoids\"");
    ASSERT_NE(at, std::string::npos);
    altered.replace(at, std::string_view("\"case_id\":\"original_295_classic_trapezoids\"").size(),
                    "\"case_id\":\"original_244_dagre_subgraphs\"");
    auto parsed = json::parse(altered);
    ASSERT_TRUE(parsed.has_value());
    OracleWork forged{kFlow400SceneLimit};
    bool exact = false;
    try {
        flow400_classic_root(parsed->root(), forged);
    } catch (const std::exception& error) {
        exact = error.what() == std::string_view("Flow400 Classic case-id/ordinal/mutation tuple");
    }
    EXPECT_TRUE(exact);
}

TEST(MermaidFlowLinkProfile3Authority, BindsOnlyFourFrozenBaseNullTuples) {
    auto docs = flow400_documents();
    auto config21 = flow400_config21_documents();
    auto typed = flow400_typed_source4_documents();
    auto classic = flow400_classic_documents();
    auto link = flow400_linkprofile3_documents();
    constexpr std::array<std::size_t, 4> ordinals{{19u, 291u, 294u, 298u}};
    for (std::size_t index = 0u; index < ordinals.size(); ++index) {
        const auto ordinal = ordinals[index];
        const auto row = link.supplement.root().get("cases").at(index);
        const auto global =
            flow400_number(row.get("base_authority").get("manifest_global_ordinal"));
        const auto fixture = docs.manifest.root().get("fixtures").at(global);
        const auto source =
            flow400_read(std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" /
                         std::string(flow400_text(fixture.get("path"))));
        OracleWork work{kFlow400SceneLimit};
        const auto prior = flow400_typed_source4_bound_authority(
            docs.compact.root().get("cases").at(ordinal), config21.supplement.root(),
            typed.supplement.root(), fixture, ordinal, global, source, work);
        const auto classic_case = flow400_classic_select(prior, classic.supplement.root(), fixture,
                                                         ordinal, global, source, work);
        ASSERT_FALSE(classic_case.is_obj());
        const auto selected = flow400_linkprofile3_select(
            prior, classic_case, link.supplement.root(), fixture, ordinal, global, source, work);
        ASSERT_TRUE(selected.is_obj());
        EXPECT_EQ(flow400_text(selected.get("case_id")), flow400_text(row.get("case_id")));
        const auto decoded = flow400_linkprofile3_model(selected, work);
        EXPECT_EQ(decoded.exact_rejections, 6u);
        EXPECT_EQ(decoded.model.edges.size(), decoded.profile.edges.size());
    }
    const auto text = link.supplement.root().to_string();
    auto forged = text;
    const auto token = forged.find("\"case_id\":\"link_profile_294\"");
    ASSERT_NE(token, std::string::npos);
    forged.replace(token, std::string_view("\"case_id\":\"link_profile_294\"").size(),
                   "\"case_id\":\"link_profile_291\"");
    auto parsed = json::parse(forged);
    ASSERT_TRUE(parsed.has_value());
    OracleWork forged_work{kFlow400SceneLimit};
    bool exact = false;
    try {
        flow400_linkprofile3_root(parsed->root(), forged_work);
    } catch (const std::exception& error) {
        exact = error.what() == std::string_view("Flow400 LinkProfile3 case-id/ordinal tuple");
    }
    EXPECT_TRUE(exact);
}

TEST(MermaidFlowLinkProfile3Oracle, HandGoodsExerciseMarkersCaptionsAndRankBands) {
    OracleWork work{kFlow400SceneLimit};
    work.spend(262144u);
    const Model endpoints{"TD",
                          {{"OpenSource", "OS"},
                           {"OpenTarget", "OT"},
                           {"ForwardSource", "FS"},
                           {"ForwardTarget", "FT"},
                           {"DoubleSource", "DS"},
                           {"DoubleTarget", "DT"}},
                          {{0, 1, "", "solid", false, false},
                           {2, 3, "", "dotted", false, true},
                           {4, 5, "", "thick", true, true}},
                          {}};
    const LinkProfile endpoint_profile{{{LinkMarker::None, LinkMarker::None, 1},
                                        {LinkMarker::None, LinkMarker::Point, 1},
                                        {LinkMarker::Point, LinkMarker::Point, 1}},
                                       {0, 1, 0, 1, 0, 1}};
    const auto endpoint_good = mock({"                                                ",
                                     "                                                ",
                                     "  ┌───────┐       ┌───────┐       ┌───────┐     ",
                                     "  │       │       │       │       │       │     ",
                                     "  │  OS   │       │  FS   │       │  DS   │     ",
                                     "  │       │       │       │       │       │     ",
                                     "  └───┬───┘       └───┬───┘       └───────┘     ",
                                     "      │               ┆               ▲         ",
                                     "      │               ┆               ┃         ",
                                     "      │               ┆               ┃         ",
                                     "      │               ┆               ┃         ",
                                     "      │               ┆               ┃         ",
                                     "      │               ┆               ┃         ",
                                     "      │               ┆               ┃         ",
                                     "      │               ▼               ▼         ",
                                     "  ┌───┴───┐       ┌───────┐       ┌───────┐     ",
                                     "  │       │       │       │       │       │     ",
                                     "  │  OT   │       │  FT   │       │  DT   │     ",
                                     "  │       │       │       │       │       │     ",
                                     "  └───────┘       └───────┘       └───────┘     ",
                                     "                                                ",
                                     "                                                ",
                                     "                                                ",
                                     "                                                ",
                                     "AFTER_FLOW_SEMANTICS                            "});
    ASSERT_TRUE(
        flow_linkprofile3_geometry(endpoint_good, endpoints, endpoint_profile, work).empty());
    const auto paired_hand_semantic = [&](const Grid& hand_good, const Model& hand_model,
                                          const LinkProfile& hand_profile, auto&& mutate,
                                          std::string_view name) {
        flow400_copy_model(hand_model, work);
        flow400_linkprofile3_copy(hand_profile, work);
        work.spend(2048u);
        auto bad_model = hand_model;
        auto bad_profile = hand_profile;
        mutate(bad_model, bad_profile);
        flow400_require(linkprofile3_profile_shape(bad_model, bad_profile),
                        "LinkProfile3 hand paired semantic Model/Profile validity");
        const auto errors = flow_linkprofile3_geometry(hand_good, bad_model, bad_profile, work);
        if (errors.empty())
            throw std::runtime_error(std::string(name) + " physical expectation admitted");
        for (const auto& error : errors)
            if (flow400_resource(error))
                throw std::runtime_error(std::string(name) + " became resource credit");
    };
    paired_hand_semantic(
        endpoint_good, endpoints, endpoint_profile,
        [](Model& bad_model, LinkProfile& bad_profile) {
            bad_model.edges[2].start = false;
            bad_profile.edges[2].source = LinkMarker::None;
        },
        "LinkProfile3 hand point-start wrong physical expectation");
    flow400_linkprofile3_expect_physical_rejected(
        endpoint_good, endpoints, endpoint_profile, work, [](Grid& bad) { bad.put({38, 7}, "┃"); },
        "LinkProfile3 hand missing double source point");
    flow400_linkprofile3_expect_physical_rejected(
        endpoint_good, endpoints, endpoint_profile, work,
        [](Grid& bad) {
            bad.put({22, 14}, "┆");
            bad.put({22, 7}, "▲");
        },
        "LinkProfile3 hand reverse forward point");
    flow400_linkprofile3_expect_physical_rejected(
        endpoint_good, endpoints, endpoint_profile, work, [](Grid& bad) { bad.put({12, 10}, "▼"); },
        "LinkProfile3 hand extra open marker");
    flow400_linkprofile3_expect_physical_rejected(
        endpoint_good, endpoints, endpoint_profile, work, [](Grid& bad) { bad.put({6, 6}, "│"); },
        "LinkProfile3 hand missing none-source contour arms");

    const auto blank_grid = [](int width, int height) {
        return Grid{width, height,
                    std::vector<std::string>(static_cast<std::size_t>(width * height), " ")};
    };
    const auto put_text = [](Grid& grid, int x, int y, std::string_view text) {
        const auto cells = ftxui::Utf8ToGlyphs(std::string(text));
        for (std::size_t at = 0u; at < cells.size(); ++at)
            grid.put({x + static_cast<int>(at), y}, cells[at]);
    };
    const auto box = [&](Grid& grid, int left, int top, int right, int bottom,
                         std::string_view label, bool source) {
        grid.put({left, top}, "┌");
        grid.put({right, top}, "┐");
        grid.put({left, bottom}, "└");
        grid.put({right, bottom}, "┘");
        for (int x = left + 1; x < right; ++x) {
            grid.put({x, top}, "─");
            grid.put({x, bottom}, "─");
        }
        for (int y = top + 1; y < bottom; ++y) {
            grid.put({left, y}, "│");
            grid.put({right, y}, "│");
        }
        put_text(grid, (left + right + 1 - static_cast<int>(label.size())) / 2, top + 2, label);
        if (source) grid.put({(left + right + 1) / 2, bottom}, "┬");
    };
    // C has two independently observed top ports and two independently
    // observed bottom ports. A fixed frame midpoint cannot bind this good Grid.
    const auto multi_port_good = [&]() {
        auto grid = blank_grid(36, 36);
        box(grid, 2, 2, 10, 6, "A", true);
        box(grid, 18, 2, 26, 6, "B", true);
        box(grid, 10, 15, 26, 19, "C", false);
        box(grid, 10, 28, 18, 32, "D", false);
        box(grid, 20, 28, 28, 32, "E", false);
        grid.put({14, 15}, "┴");
        grid.put({22, 15}, "┴");
        grid.put({14, 19}, "┬");
        grid.put({22, 19}, "┬");
        for (int y = 7; y <= 9; ++y)
            grid.put({6, y}, "│");
        grid.put({6, 10}, "└");
        for (int x = 7; x <= 13; ++x)
            grid.put({x, 10}, "─");
        grid.put({14, 10}, "┐");
        for (int y = 11; y <= 14; ++y)
            grid.put({14, y}, "│");
        for (int y = 7; y <= 14; ++y)
            grid.put({22, y}, "┆");
        for (int y = 20; y <= 26; ++y)
            grid.put({14, y}, "│");
        grid.put({14, 27}, "▼");
        for (int y = 20; y <= 26; ++y)
            grid.put({22, y}, "┃");
        grid.put({22, 27}, "▼");
        put_text(grid, 0, 35, kSentinel);
        return grid;
    };
    const Model multi_ports{"TD",
                            {{"A", "A"}, {"B", "B"}, {"C", "C"}, {"D", "D"}, {"E", "E"}},
                            {{0, 2, "", "solid", false, false},
                             {1, 2, "", "dotted", false, false},
                             {2, 3, "", "solid", false, true},
                             {2, 4, "", "thick", false, true}},
                            {}};
    const LinkProfile multi_port_profile{{{LinkMarker::None, LinkMarker::None, 1},
                                          {LinkMarker::None, LinkMarker::None, 1},
                                          {LinkMarker::None, LinkMarker::Point, 1},
                                          {LinkMarker::None, LinkMarker::Point, 1}},
                                         {0, 0, 1, 2, 2}};
    const auto multi_ports_grid = multi_port_good();
    ASSERT_TRUE(flow_linkprofile3_geometry(multi_ports_grid, multi_ports, multi_port_profile, work)
                    .empty());
    flow400_linkprofile3_expect_physical_rejected(
        multi_ports_grid, multi_ports, multi_port_profile, work,
        [](Grid& bad) { bad.put({22, 15}, "─"); },
        "LinkProfile3 multi-in missing distinct target port");
    flow400_linkprofile3_expect_physical_rejected(
        multi_ports_grid, multi_ports, multi_port_profile, work,
        [](Grid& bad) { bad.put({22, 19}, "─"); },
        "LinkProfile3 multi-out missing distinct source port");

    const auto caption_good = [&]() {
        auto grid = blank_grid(64, 25);
        box(grid, 2, 2, 10, 6, "A", true);
        box(grid, 20, 15, 28, 19, "B", false);
        box(grid, 34, 2, 42, 6, "C", true);
        box(grid, 52, 15, 60, 19, "D", false);
        for (int y = 7; y <= 9; ++y) {
            grid.put({6, y}, "│");
            grid.put({38, y}, "┆");
        }
        grid.put({6, 10}, "└");
        for (int x = 7; x <= 12; ++x)
            grid.put({x, 10}, "─");
        put_text(grid, 13, 10, "alpha");
        for (int x = 18; x <= 23; ++x)
            grid.put({x, 10}, "─");
        grid.put({24, 10}, "┐");
        for (int y = 11; y <= 13; ++y)
            grid.put({24, y}, "│");
        grid.put({24, 14}, "▼");
        grid.put({38, 10}, "└");
        for (int x = 39; x <= 44; ++x)
            grid.put({x, 10}, "┄");
        put_text(grid, 45, 10, "beta");
        for (int x = 49; x <= 55; ++x)
            grid.put({x, 10}, "┄");
        grid.put({56, 10}, "┐");
        for (int y = 11; y <= 13; ++y)
            grid.put({56, y}, "┆");
        grid.put({56, 14}, "▼");
        put_text(grid, 0, 24, kSentinel);
        return grid;
    };
    const Model captions{
        "TD",
        {{"A", "A"}, {"B", "B"}, {"C", "C"}, {"D", "D"}},
        {{0, 1, "alpha", "solid", false, true}, {2, 3, "beta", "dotted", false, true}},
        {}};
    const LinkProfile caption_profile{
        {{LinkMarker::None, LinkMarker::Point, 1}, {LinkMarker::None, LinkMarker::Point, 1}},
        {0, 1, 0, 1}};
    const auto caption = caption_good();
    ASSERT_TRUE(flow_linkprofile3_geometry(caption, captions, caption_profile, work).empty());
    flow400_linkprofile3_expect_model_rejected(
        caption, captions, caption_profile, work,
        [](Model& bad) { bad.edges[0].label = "alpha absent"; },
        "LinkProfile3 hand source label semantic");
    flow400_linkprofile3_expect_physical_rejected(
        caption, captions, caption_profile, work,
        [&](Grid& bad) { put_text(bad, 45, 10, "alpha"); }, "LinkProfile3 hand caption reuse");
    flow400_linkprofile3_expect_physical_rejected(
        caption, captions, caption_profile, work,
        [&](Grid& bad) { put_text(bad, 30, 10, "spare"); }, "LinkProfile3 hand spare caption");
    flow400_linkprofile3_expect_physical_rejected(
        caption, captions, caption_profile, work, [](Grid& bad) { bad.put({63, 0}, "┼"); },
        "LinkProfile3 hand orphan route ink");

    const auto minimum_grid = [&](int target_top) {
        auto grid = blank_grid(24, 38);
        box(grid, 2, 2, 12, 6, "S", true);
        box(grid, 2, target_top, 12, target_top + 4, "T", false);
        const int middle = 7;
        for (int y = 7; y < target_top - 1; ++y)
            grid.put({middle, y}, "│");
        grid.put({middle, target_top - 1}, "▼");
        put_text(grid, 0, 37, kSentinel);
        return grid;
    };
    const Model minimum{
        "TD", {{"ShortSource", "S"}, {"LongTarget", "T"}}, {{0, 1, "", "solid", false, true}}, {}};
    const LinkProfile minimum_profile{{{LinkMarker::None, LinkMarker::Point, 3}}, {0, 3}};
    const auto long_grid = minimum_grid(31);
    ASSERT_TRUE(flow_linkprofile3_geometry(long_grid, minimum, minimum_profile, work).empty());
    flow400_require(
        minimum.edges[0u].to == 1 &&
            std::ranges::none_of(minimum.edges, [](const Edge& edge) { return edge.from == 1; }),
        "LinkProfile3 hand minimum target leaf witness");
    paired_hand_semantic(
        long_grid, minimum, minimum_profile,
        [](Model&, LinkProfile& bad_profile) {
            ++bad_profile.edges[0u].minimum_length;
            ++bad_profile.node_ranks[1u];
        },
        "LinkProfile3 hand minimum-length wrong physical expectation");
    flow400_linkprofile3_expect_physical_rejected(
        long_grid, minimum, minimum_profile, work, [&](Grid& bad) { bad = minimum_grid(23); },
        "LinkProfile3 hand one source rank band short");
    // The following-row protocol rejects a truncated token by its exact
    // exception contract, before geometry returns an error vector.
    flow400_copy_grid(long_grid, work);
    auto truncated = long_grid;
    for (std::size_t at = 0u; at < kSentinel.size(); ++at)
        truncated.put({static_cast<int>(at), 37},
                      at < 15u ? std::string(kSentinel.substr(at, 1u)) : " ");
    bool truncated_rejected = false;
    try {
        static_cast<void>(flow_linkprofile3_geometry(truncated, minimum, minimum_profile, work));
    } catch (const std::runtime_error& error) {
        truncated_rejected =
            std::string_view(error.what()) == "Flow400 protocol exact independent following row";
    }
    EXPECT_TRUE(truncated_rejected);
    OracleWork zero{0u};
    bool resource = false;
    try {
        static_cast<void>(
            flow_linkprofile3_geometry(endpoint_good, endpoints, endpoint_profile, zero));
    } catch (const std::exception& error) {
        resource = flow400_resource(error.what());
    }
    EXPECT_TRUE(resource);
}

TEST(MermaidCapabilityReport, Flow400CompleteSources) {
    auto docs = flow400_documents();
    auto supplemental = flow400_config21_documents();
    auto typed_source4 = flow400_typed_source4_documents();
    auto classic = flow400_classic_documents();
    auto linkprofile3 = flow400_linkprofile3_documents();
    auto group_endpoint15 = group_endpoint15_documents();
    std::size_t selected_sources = 0u, selected_typed_source4 = 0u, selected_classic = 0u,
                selected_linkprofile3 = 0u, selected_group_endpoint15 = 0u, selected_title = 0u,
                selected_title_margin = 0u, selected_issue4648 = 0u, selected_shape_alias = 0u;
    OracleWork report_work;
    report_work.spend(65536u + 4u * sizeof(json::JsonMutDoc));
    RecordProperty("flow400_sizeof_Pixel", std::to_string(sizeof(ftxui::Pixel)));
    RecordProperty("flow400_sizeof_PixelRow", std::to_string(sizeof(std::vector<ftxui::Pixel>)));
    RecordProperty("flow400_sizeof_string", std::to_string(sizeof(std::string)));
    RecordProperty("flow400_sizeof_Grid", std::to_string(sizeof(Grid)));
    RecordProperty("flow400_sizeof_Model", std::to_string(sizeof(Model)));
    RecordProperty("flow400_sizeof_JsonDoc", std::to_string(sizeof(json::JsonDoc)));
    RecordProperty("flow400_sizeof_JsonMutVal", std::to_string(sizeof(json::JsonMutVal)));
    json::JsonMutDoc report;
    auto root = report.object(), rows = root.make_arr();
    root.set("kind", "official_flow400_complete_source_native_test_first");
    root.set("qualification", "Full original source facts; terminal whole-Model only; foreign "
                              "certification remains zero.");
    root.set("scene_limit", static_cast<int64_t>(kFlow400SceneLimit));
    root.set("compact_setup_remaining", static_cast<int64_t>(docs.compact_work.remaining));
    const auto fixtures = docs.manifest.root().get("fixtures"),
               cases = docs.compact.root().get("cases");
    std::size_t visited = 0, decoded = 0, pending = 0, positive = 0, mutated = 0, errors = 0,
                unmet = 0, geometry_failures = 0, mutation_failures = 0;
    std::size_t global_ordinal = 0;
    fixtures.iter([&](JsonVal fixture) {
        docs.manifest_work.spend(128u);
        const auto global = global_ordinal++;
        const auto path = flow400_text(fixture.get("path"));
        docs.manifest_work.spend(path.size());
        if (!path.starts_with("flowchart/")) return;
        const auto ordinal = visited++;
        OracleWork work{kFlow400SceneLimit};
        work.spend(65536u + 4u * sizeof(Flow400Observation) + 64u * sizeof(json::JsonMutVal) +
                   16u * sizeof(std::string) + 8192u + 8u * sizeof(std::size_t) +
                   4u * sizeof(json::JsonVal) + 4u * 512u);
        const bool issue4648_selected = ordinal == kFlow400Issue4648Ordinal;
        if (issue4648_selected)
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(Model) +
                       32u * sizeof(ShapeFrame) + 1024u);
        const bool title_margin_selected = ordinal == kFlowTitleMarginOrdinal;
        if (title_margin_selected)
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(Model) +
                       32u * sizeof(ShapeFrame) + 1024u);
        const bool shape_alias_selected = flow_shape_alias_case(ordinal) != nullptr;
        if (shape_alias_selected)
            work.spend(65536u + 16u * sizeof(json::JsonVal) + 16u * sizeof(Model) +
                       32u * sizeof(ShapeFrame) + 1024u);
        Flow400Observation observation;
        auto row = root.make_obj();
        row.set("ordinal", static_cast<int64_t>(ordinal));
        row.set("fixture", path);
        row.set("source_sha256", flow400_text(fixture.get("sha256")));
        row.set("manifest_global_ordinal", static_cast<int64_t>(global));
        row.set("source_git_blob", flow400_text(fixture.get("git_blob")));
        row.set("source_bytes", static_cast<int64_t>(flow400_number(fixture.get("bytes"))));
        bool source_wide = false, source_narrow = false, restored_same = false;
        std::size_t rejects = 0;
        const char* phase = "source";
        try {
            const auto bytes = flow400_number(fixture.get("bytes"));
            work.product(bytes, 32u);
            work.product(path.size(), 8u);
            work.spend(16384u);
            const auto source = flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                                             "fixtures/mermaid_official/cases" / std::string(path));
            flow400_require(source.size() == bytes &&
                                digest(source) == flow400_text(fixture.get("sha256")),
                            "Flow400 protocol original source read");
            SCOPED_TRACE(std::string(path));
            auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
            element->ComputeRequirement();
            ordinary->ComputeRequirement();
            const auto line_floor =
                static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 12;
            const int height =
                std::min(1040, std::max({32, element->requirement().min_y + 8,
                                         ordinary->requirement().min_y + 8, line_floor}));
            row.set("height", height);
            phase = "capture";
            const std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            constexpr std::array<std::string_view, 5> names{
                {"wide_frame", "narrow_frame", "restored_frame", "ordinary_wide_frame",
                 "ordinary_narrow_frame"}};
            for (std::size_t n = 0; n < 5u; ++n) {
                observation.frames[n] =
                    capture(n < 3u ? element : ordinary, widths[n], height, &work);
                ++observation.captures;
                flow400_publication(*observation.frames[n], work);
                flow400_append_frame(row, names[n], *observation.frames[n]);
                ++observation.published;
            }
            flow400_require(observation.captures == 5u && observation.published == 5u,
                            "Flow400 full frames attached before source binding");
            observation.remaining[0] = work.remaining;
            phase = "following";
            for (const auto& frame : observation.frames)
                static_cast<void>(flow400_following(*frame, work));
            source_wide = flow400_same(*observation.frames[0], *observation.frames[3], work);
            source_narrow = flow400_same(*observation.frames[1], *observation.frames[4], work);
            restored_same = flow400_same(*observation.frames[0], *observation.frames[2], work);
            observation.remaining[1] = work.remaining;
            phase = "bind";
            const auto selected = flow400_typed_source4_bound_authority(
                cases.at(ordinal), supplemental.supplement.root(), typed_source4.supplement.root(),
                fixture, ordinal, global, source, work);
            const bool title_selected = ordinal == kFlowTitleOrdinal;
            std::optional<Model> title_model;
            if (title_selected) {
                flow_title_bind(fixture, ordinal, global, source, work);
                title_model.emplace(flow_title_model(work));
                ++selected_title;
            }
            if (title_margin_selected) {
                flow_title_margin_bind(fixture, ordinal, global, source, work);
                ++selected_title_margin;
            }
            if (shape_alias_selected) {
                flow_shape_alias_bind(docs, fixture, ordinal, global, source, work);
                ++selected_shape_alias;
            }
            if (issue4648_selected) {
                flow400_issue4648_bind(docs, source, work);
                ++selected_issue4648;
            }
            const auto classic_case = flow400_classic_select(
                selected, classic.supplement.root(), fixture, ordinal, global, source, work);
            const bool classic_selected = classic_case.is_obj();
            const auto link_case = classic_selected
                                       ? JsonVal{}
                                       : flow400_linkprofile3_select(
                                             selected, classic_case, linkprofile3.supplement.root(),
                                             fixture, ordinal, global, source, work);
            const bool link_selected = link_case.is_obj();
            std::optional<BoundGroupEndpoint15> group_selection;
            if (group_endpoint15_candidate_ordinal(ordinal)) {
                group_selection = group_endpoint15_select(
                    cases.at(ordinal), fixture, group_endpoint15.asset.root().get("records"),
                    ordinal, global, source, work);
                flow400_require(group_selection.has_value(),
                                "Flow400 GroupEndpoint15 selected tuple missing");
            }
            const bool group_selected = group_selection.has_value();
            const auto authority = group_selected     ? group_selection->authority
                                   : link_selected    ? link_case.get("base_authority")
                                   : classic_selected ? classic_case.get("base_authority")
                                                      : selected.authority;
            if (selected.kind == Flow400TypedSource4Kind::Config21) ++selected_sources;
            if (selected.kind == Flow400TypedSource4Kind::TypedSource4) ++selected_typed_source4;
            if (title_selected) {
                row.set("source_authority", "immutable_base8_plus_DiagramTitle1");
                row.set("title_model_sha256", std::string(kFlowTitleModelSha));
            } else if (title_margin_selected) {
                row.set("source_authority", "immutable_base8_plus_TitleTopMargin1");
                row.set("title_top_margin_model_sha256", std::string(kFlowTitleMarginModelSha));
            } else if (shape_alias_selected) {
                row.set("source_authority", "immutable_base8_plus_FlowShapeAlias16");
                row.set("flow_shape_alias_model_sha256",
                        std::string(flow_shape_alias_case(ordinal)->model_sha));
            } else if (issue4648_selected) {
                row.set("source_authority", "immutable_base8_plus_Issue4648");
                row.set("source_facts_sha256", std::string(kFlow400Issue4648FactsSha));
            } else if (group_selected) {
                ++selected_group_endpoint15;
                row.set("source_authority", "immutable_base8_plus_GroupEndpoint15");
                row.set("source_supplement_sha256", kFlow400GroupEndpoint15AssetSha);
                row.set("group_endpoint15_case",
                        flow400_text(group_selection->record.get("case_id")));
                row.set("group_endpoint15_expected_rejections",
                        static_cast<int64_t>(
                            group_endpoint15_expected_mutations(group_selection->record)));
            } else if (link_selected) {
                ++selected_linkprofile3;
                row.set("source_authority", "immutable_base8_plus_linkprofile3");
                row.set("source_supplement_sha256", kFlow400LinkProfile3AssetSha);
                row.set("linkprofile3_case", flow400_text(link_case.get("case_id")));
                row.set("linkprofile3_expected_rejections",
                        static_cast<int64_t>(flow400_linkprofile3_expected(link_case)));
            } else if (classic_selected) {
                ++selected_classic;
                row.set("source_authority", "immutable_base8_plus_classic");
                row.set("source_supplement_sha256", kFlow400ClassicAssetSha);
                row.set("classic_case", flow400_text(classic_case.get("case_id")));
                row.set("classic_expected_rejections",
                        static_cast<int64_t>(flow400_classic_expected(classic_case)));
            } else if (selected.kind == Flow400TypedSource4Kind::TypedSource4) {
                row.set("source_authority", "immutable_base8_plus_typed_source4");
                row.set("source_supplement_sha256", kFlow400TypedSource4AssetSha);
                row.set("typed_source4_case", flow400_text(selected.typed_case.get("case_id")));
                row.set("typed_source4_expected_rejections",
                        static_cast<int64_t>(selected.exact_rejections));
            } else if (selected.kind == Flow400TypedSource4Kind::Config21) {
                row.set("source_authority", "immutable_base8_plus_config21");
                row.set("source_supplement_sha256", kFlow400Config21AssetSha);
            } else {
                row.set("source_authority", "immutable_base8");
                row.add("source_supplement_sha256", row.make_null());
            }
            std::optional<Model> model;
            std::optional<LinkProfile> profile;
            std::optional<GroupEndpoint15Model> group_model;
            if (title_selected)
                model = std::move(title_model);
            else if (title_margin_selected)
                model.emplace(flow_title_margin_model(work));
            else if (shape_alias_selected)
                model.emplace(flow_shape_alias_model(ordinal, work));
            else if (issue4648_selected)
                model.emplace(flow400_issue4648_model(work));
            else if (group_selected) {
                group_model.emplace(
                    group_endpoint15_model(group_selection->record.get("model"), work));
                model.emplace(group_model->base);
            } else if (link_selected) {
                auto link_decoded = flow400_linkprofile3_model(link_case, work);
                model.emplace(std::move(link_decoded.model));
                profile.emplace(std::move(link_decoded.profile));
            } else if (classic_selected)
                model.emplace(flow400_classic_model(classic_case, work));
            else
                model = flow400_typed_source4_model(selected, work);
            observation.model_decoded = true;
            observation.remaining[2] = work.remaining;
            row.set("source_git_blob", flow400_text(authority.get("source_git_blob")));
            row.set("oracle_record_sha256", flow400_text(authority.get("oracle_record_sha256")));
            if (!model) {
                ++pending;
            } else {
                ++decoded;
                row.set("model_sha256",
                        title_selected          ? std::string(kFlowTitleModelSha)
                        : title_margin_selected ? std::string(kFlowTitleMarginModelSha)
                        : shape_alias_selected
                            ? std::string(flow_shape_alias_case(ordinal)->model_sha)
                        : issue4648_selected ? std::string(kFlow400Issue4648ModelSha)
                        : group_selected
                            ? flow400_text(group_selection->record.get("canonical_model_sha256"))
                        : link_selected    ? flow400_text(link_case.get("link_model_sha256"))
                        : classic_selected ? flow400_text(classic_case.get("classic_model_sha256"))
                        : selected.kind == Flow400TypedSource4Kind::TypedSource4
                            ? flow400_text(selected.typed_case.get("typed_model_sha256"))
                            : flow400_text(authority.get("model_sha256")));
                phase = "geometry";
                if (source_wide) {
                    observation.positive_unmet = true;
                    throw std::runtime_error("Flow400 mandatory positive returned whole source");
                }
                const auto wide_errors =
                    title_selected   ? flow_title_geometry(*observation.frames[0], *model, work)
                    : group_selected ? group_endpoint15_geometry(*observation.frames[0],
                                                                 *group_model, work, kSentinel)
                    : link_selected
                        ? flow_linkprofile3_geometry(*observation.frames[0], *model, *profile, work)
                        : flow400_geometry(*observation.frames[0], *model, work);
                if (!wide_errors.empty())
                    throw std::runtime_error(
                        std::string(std::string_view(wide_errors.front()).substr(0, 256u)));
                if (!source_narrow) {
                    const auto narrow_errors =
                        title_selected   ? flow_title_geometry(*observation.frames[1], *model, work)
                        : group_selected ? group_endpoint15_geometry(*observation.frames[1],
                                                                     *group_model, work, kSentinel)
                        : link_selected ? flow_linkprofile3_geometry(*observation.frames[1], *model,
                                                                     *profile, work)
                                        : flow400_geometry(*observation.frames[1], *model, work);
                    if (!narrow_errors.empty())
                        throw std::runtime_error(
                            std::string(std::string_view(narrow_errors.front()).substr(0, 256u)));
                }
                flow400_require(restored_same, "Flow400 protocol full restored cells differ");
                ++positive;
                observation.remaining[3] = work.remaining;
                phase = "mutations";
                rejects =
                    title_selected   ? flow_title_mutations(*observation.frames[0], *model, work)
                    : group_selected ? group_endpoint15_mutations(*observation.frames[0],
                                                                  *group_model, work, kSentinel)
                    : link_selected ? flow400_linkprofile3_mutations(*observation.frames[0], *model,
                                                                     *profile, link_case, work)
                    : classic_selected ? flow400_classic_mutations(*observation.frames[0], *model,
                                                                   classic_case, work)
                    : selected.kind == Flow400TypedSource4Kind::TypedSource4
                        ? flow400_typed_source4_mutations(*observation.frames[0], *model,
                                                          selected.typed_case, work)
                        : flow400_mutations(*observation.frames[0], *model, work);
                if (group_selected)
                    flow400_require(
                        rejects == group_endpoint15_expected_mutations(group_selection->record),
                        "Flow400 GroupEndpoint15 exact mutation execution");
                else if (title_selected)
                    flow400_require(rejects == 4u,
                                    "Flow400 diagram title exact mutation execution");
                else if (shape_alias_selected)
                    flow400_require(rejects == 2u, "Flow400 shape-alias exact mutation execution");
                else if (link_selected)
                    flow400_require(rejects == flow400_linkprofile3_expected(link_case),
                                    "Flow400 LinkProfile3 exact mutation execution");
                else if (classic_selected)
                    flow400_require(rejects == flow400_classic_expected(classic_case),
                                    "Flow400 Classic exact mutation execution");
                else if (selected.kind == Flow400TypedSource4Kind::TypedSource4)
                    flow400_require(rejects == selected.exact_rejections,
                                    "Flow400 TypedSource4 exact mutation execution");
                else
                    flow400_require(rejects > 0u, "Flow400 missing meaningful mutations");
                ++mutated;
                observation.whole = true;
                observation.remaining[4] = work.remaining;
            }
            flow400_require(restored_same, "Flow400 protocol whole source restoration differs");
        } catch (const std::exception& error) {
            std::size_t n = 0;
            for (; n < 256u && error.what()[n]; ++n)
                observation.error[n] = error.what()[n];
            const auto message = std::string_view(observation.error.data(), n);
            observation.resource = work.remaining == 0u || flow400_resource(message);
            if (observation.positive_unmet)
                ++unmet;
            else if (observation.resource || message.starts_with("Flow400 protocol"))
                ++errors;
            else if (std::string_view(phase) == "geometry")
                ++geometry_failures;
            else if (std::string_view(phase) == "mutations")
                ++mutation_failures;
            else
                ++errors;
        }
        // Row/scalar/error reserve and each captured full frame's publication
        // were paid before a semantic phase could exhaust this scene ledger.
        row.set("positive_unmet", observation.positive_unmet);
        row.set("phase", phase);
        row.set("error", std::string_view(observation.error.data()));
        row.set("resource_error", observation.resource);
        row.set("capture_count", static_cast<int64_t>(observation.captures));
        row.set("published_frames", static_cast<int64_t>(observation.published));
        row.set("expectation_decoded", observation.model_decoded);
        row.set("source_wide", source_wide);
        row.set("source_narrow", source_narrow);
        row.set("restored_identically", restored_same);
        row.set("whole_source_admitted", observation.whole);
        row.set("semantic_mutations_rejected", static_cast<int64_t>(rejects));
        row.set("remaining", static_cast<int64_t>(work.remaining));
        auto stages = row.make_arr();
        for (const auto left : observation.remaining)
            stages.append(row.make_num(static_cast<int64_t>(left)));
        row.add("phase_remaining", stages);
        rows.append(row);
    });
    EXPECT_EQ(global_ordinal, 2145u);
    root.set("manifest_setup_remaining", static_cast<int64_t>(docs.manifest_work.remaining));
    root.set("visited", static_cast<int64_t>(visited));
    root.set("complete_models_decoded", static_cast<int64_t>(decoded));
    root.set("whole_pending", static_cast<int64_t>(pending));
    root.set("complete_positive_geometry", static_cast<int64_t>(positive));
    root.set("mutated_positive", static_cast<int64_t>(mutated));
    root.set("errors", static_cast<int64_t>(errors));
    root.set("positive_source_unmet", static_cast<int64_t>(unmet));
    root.set("geometry_failures", static_cast<int64_t>(geometry_failures));
    root.set("mutation_failures", static_cast<int64_t>(mutation_failures));
    root.set("selected_config21_sources", static_cast<int64_t>(selected_sources));
    root.set("selected_typed_source4_sources", static_cast<int64_t>(selected_typed_source4));
    root.set("selected_classic_sources", static_cast<int64_t>(selected_classic));
    root.set("selected_linkprofile3_sources", static_cast<int64_t>(selected_linkprofile3));
    root.set("selected_group_endpoint15_sources", static_cast<int64_t>(selected_group_endpoint15));
    root.set("selected_diagram_title_sources", static_cast<int64_t>(selected_title));
    root.set("selected_title_top_margin_sources", static_cast<int64_t>(selected_title_margin));
    root.set("selected_issue4648_sources", static_cast<int64_t>(selected_issue4648));
    root.set("selected_shape_alias_sources", static_cast<int64_t>(selected_shape_alias));
    root.set("config21_supplement_sha256", kFlow400Config21AssetSha);
    root.set("typed_source4_supplement_sha256", kFlow400TypedSource4AssetSha);
    root.set("classic_supplement_sha256", kFlow400ClassicAssetSha);
    root.set("linkprofile3_supplement_sha256", kFlow400LinkProfile3AssetSha);
    root.set("group_endpoint15_supplement_sha256", kFlow400GroupEndpoint15AssetSha);
    root.set("foreign_certified", 0);
    // RFC 0031 E6c: capability report, not an aspiration gate. The 400
    // complete-source target and the authority-determined pending rows
    // are explicit frozen fields; no roster is needed because pending/decoded
    // membership is set by immutable Model authority assets.
    root.set("complete_source_target", 400);
    root.set("mandatory_pending_remains", static_cast<int64_t>(pending));
    root.add("cases", rows);
    report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto target = std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                        "mermaid-flow-official400-whole-source.json";
    std::ofstream output(target, std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n')) << "Cannot publish " << target;
#endif
    EXPECT_EQ(selected_sources, 21u);
    EXPECT_EQ(selected_typed_source4, 4u);
    EXPECT_EQ(selected_classic, 6u);
    EXPECT_EQ(selected_linkprofile3, 4u);
    EXPECT_EQ(selected_group_endpoint15, 15u);
    EXPECT_EQ(selected_title, 1u);
    EXPECT_EQ(selected_title_margin, 1u);
    EXPECT_EQ(selected_issue4648, 1u);
    EXPECT_EQ(selected_shape_alias, 16u);
    EXPECT_EQ(visited, 400u);
    EXPECT_EQ(decoded, 77u);
    EXPECT_EQ(pending, 323u);
    EXPECT_EQ(positive, 77u);
    EXPECT_EQ(mutated, 77u);
    EXPECT_EQ(errors, 0u);
    EXPECT_EQ(unmet, 0u);
    EXPECT_EQ(geometry_failures, 0u);
    EXPECT_EQ(mutation_failures, 0u);
}
}  // namespace

namespace {
struct Flow6EnvelopeControl {
    std::string_view name, source, source_sha256;
};

constexpr std::array<Flow6EnvelopeControl, 8> kFlow6EnvelopePositive{{
    {"description_only", R"FLOW6(---
description:
  Flow.
---
flowchart LR
A --> B
)FLOW6",
     "f62f11b10120b5934d60735f04373c6e8ffc5080a4311df58b8f72ce11329751"},
    {"arrow_initial", R"FLOW6(---
description:
  -> brief
---
flowchart LR
A --> B
)FLOW6",
     "fa71dead57fe9fefe48091b7ac4622d71331395bc0892793e12b5f9ab5cdd912"},
    {"inner_ampersand_apostrophe", R"FLOW6(---
description:
  dagre's source & order
---
flowchart LR
A --> B
)FLOW6",
     "069179340cdb5182d4991f73eed3ca0642ea457420cd0a2678afc9dd169aeb98"},
    {"dagre_bare", R"FLOW6(---
description:
  Flow.
config:
  layout: dagre
---
flowchart LR
A --> B
)FLOW6",
     "1d682ce018c1de3efb870d4c2cffbf876f5ee953b90fb786ac3d7f8321f88edf"},
    {"dagre_quoted_config_first", R"FLOW6(---
config:
  layout: "dagre"
description:
  Flow.
---
flowchart LR
A --> B
)FLOW6",
     "c1e4750aeda7d38e3e62f316473e00689db52f482ba590f20e35491f1a27bb79"},
    {"crlf_whole", "---\r\ndescription:\r\n  Flow.\r\n---\r\nflowchart LR\r\nA --> B\r\n",
     "8fce6652d1c1791953dbc109d2c8badfe67d91138ad2c6035edce86b7f424c39"},
    {"dagre_config_only", R"FLOW6(---
config:
  layout: dagre
---
flowchart LR
A --> B
)FLOW6",
     "573338f5b25fd2c371499c4e55e3a7e7517d29fa76a88002392aa7ea0a62ab51"},
    {"opaque_unicode", R"FLOW6(---
description:
  保留说明文本
---
flowchart LR
A --> B
)FLOW6",
     "c588dab034f21ef0eb0ad7ab95d0ecf70b0666c871bee2af21d214541d2061e1"},
}};
constexpr std::array<Flow6EnvelopeControl, 44> kFlow6EnvelopeClosed{{
    {"initial_byte_44", R"FLOW6(---
description:
  , forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "120f3bc1d8d254f0d262a1f41c7e69f6ee3502afd4b47acd210307f2a3d79997"},
    {"initial_byte_91", R"FLOW6(---
description:
  [ forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "3f9d1152ebb79b505db4835a493fc047170592b255b8fae69eafe117bab383b2"},
    {"initial_byte_93", R"FLOW6(---
description:
  ] forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "4487d2ec09c3abbbdb9723a5cdcbf3ef7865d8df8c0284e9bfc90cd12093ab21"},
    {"initial_byte_123", R"FLOW6(---
description:
  { forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "bf9775e120e4df88f381fb35d325ad726602d57d76f9e90789f18ebc874e13bb"},
    {"initial_byte_125", R"FLOW6(---
description:
  } forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "be44ab6d7719fb235f7ad61a71e1fc22fedd12d909b335d1a5efba7bc06cdb5a"},
    {"initial_byte_35", R"FLOW6(---
description:
  # forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "f53c13e8c6c79135f971bb8a24fe73748e7d3e21f9f6ed4827b68d84004dd992"},
    {"initial_byte_38", R"FLOW6(---
description:
  & forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "8d6733ca2bbbf17f826bf57ffb83edf06b0d6ff2ca4fc9d7b8e72d19a7564a71"},
    {"initial_byte_42", R"FLOW6(---
description:
  * forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "1b0c17eddf438869655fa10655ec676f2d7ef64d968e714713ac8eb879e49960"},
    {"initial_byte_33", R"FLOW6(---
description:
  ! forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "44c8247366fee29365ecffbe4d46e6d0870df854afb7a47b87724042c2a11bac"},
    {"initial_byte_124", R"FLOW6(---
description:
  | forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "c6d771ef657cd65d2b7f3746f8b3d10caba9e078480eb15bf711e19a369ef88d"},
    {"initial_byte_62", R"FLOW6(---
description:
  > forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "b4ce3d47f16cf40141184c3fbec0d43992edfbf1fbbf5997333e9a9234214569"},
    {"initial_byte_39", R"FLOW6(---
description:
  ' forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "bd2fd0bb8712e6f2e0745def32847a18f187835dab124b06179d7917c3bcc481"},
    {"initial_byte_34", R"FLOW6(---
description:
  " forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "2d2d98fbc8335a7b3aa405c9bbc62ba6eaeadb25ea55fd47e3f01ab19ebab240"},
    {"initial_byte_37", R"FLOW6(---
description:
  % forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "4d38eea627e510065aa816dfc17c5778769408345f55ccdad56be8d6326b92b4"},
    {"initial_byte_64", R"FLOW6(---
description:
  @ forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "0a14a7395b773c0bddd9f3f3f2fc44daddd9ae5c3ba05137cae8911b9d871c20"},
    {"initial_byte_96", R"FLOW6(---
description:
  ` forbidden opener
---
flowchart LR
A --> B
)FLOW6",
     "18d68c5dba2078297fbdbac323a346edf488cd8bdeb19d4f76361d620e3e2e83"},
    {"separated_dash", R"FLOW6(---
description:
  - item
---
flowchart LR
A --> B
)FLOW6",
     "e75f7b45d3d28ce75c13d752f546842ccb83385bea5e1ad3104a203c6a26c3d9"},
    {"separated_question", R"FLOW6(---
description:
  ? item
---
flowchart LR
A --> B
)FLOW6",
     "f78dea433cd2f6c956fd0e5285761069c00fd24591226bcd98bf029143e5c736"},
    {"dash_at_end", R"FLOW6(---
description:
  -
---
flowchart LR
A --> B
)FLOW6",
     "1e0819a17844fd1ac64716413b763c21c5c87d82b12912415c379b8d36e572fd"},
    {"unknown_root", R"FLOW6(---
description:
  Flow.
unknown: true
---
flowchart LR
A --> B
)FLOW6",
     "d8bf256bf8a6a7ed955f70b3f0855ac1e3735c9fb1be8170575015663ed41d51"},
    {"visible_title", R"FLOW6(---
description:
  Flow.
title: Visible
---
flowchart LR
A --> B
)FLOW6",
     "42147ba8ac50958932970e2ed49481391df815c05ba7fdf105268c81bbddd54e"},
    {"title_only_frontmatter", R"FLOW6(---
title: Visible
---
flowchart LR
A --> B
)FLOW6",
     "3fbfbd6e807f4d4b4eb27729d3fce62ed08c8ad9e6231a3fb779ec5aebc94afe"},
    {"duplicate_description", R"FLOW6(---
description:
  Flow.
description:
  More.
---
flowchart LR
A --> B
)FLOW6",
     "a74baf3d2060e4073f926765b54e3aa1250bad8141b623ad0d1a2ecf06710400"},
    {"duplicate_config", R"FLOW6(---
config:
  layout: dagre
config:
  layout: dagre
---
flowchart LR
A --> B
)FLOW6",
     "af641fee35e45b458c4ffe72b8938230e732723cbe173a8edfdd5244fc299ea8"},
    {"duplicate_layout", R"FLOW6(---
config:
  layout: dagre
  layout: dagre
---
flowchart LR
A --> B
)FLOW6",
     "7b1e3012e4672f1e80daa8df3e0d0095105d1d9986d153a45b93a618b0cf0639"},
    {"mixed_active_config", R"FLOW6(---
config:
  layout: dagre
  htmlLabels: true
---
flowchart LR
A --> B
)FLOW6",
     "b47dcd514e01a7c0312f23b8d5976c041f1f8bb6638f5440855904a64cc4eaa6"},
    {"unknown_nested", R"FLOW6(---
config:
  layout: dagre
  unknown: true
---
flowchart LR
A --> B
)FLOW6",
     "e1825d8be514d136e07b8f5c70a0bc2db1c02298c3abd335a09a9c6014700015"},
    {"different_layout", R"FLOW6(---
config:
  layout: elk
---
flowchart LR
A --> B
)FLOW6",
     "6bfdde91877944b0b04172c071975fa6efb3a46107f862f58f5bf4787c9b4bdc"},
    {"layout_boolean", R"FLOW6(---
config:
  layout: true
---
flowchart LR
A --> B
)FLOW6",
     "02465bc99f8f68e2de8649a9f3e000332280b0bdfb256a2d968dbac80896a5ad"},
    {"layout_null", R"FLOW6(---
config:
  layout: null
---
flowchart LR
A --> B
)FLOW6",
     "1184e91558a53917436272e84d50050e623b676a4dbc273790fd8033d23f360c"},
    {"layout_single_quote", R"FLOW6(---
config:
  layout: 'dagre'
---
flowchart LR
A --> B
)FLOW6",
     "288be1a54907fc7b3471544fc1fd8e7168b486d2a176d951846293efe27bfe42"},
    {"layout_trailing_ink", R"FLOW6(---
config:
  layout: "dagre" extra
---
flowchart LR
A --> B
)FLOW6",
     "cefca72b69f46cf2b639e8f4359b2ae1073dfcaf92ebebacc5e94cc4dfdacba1"},
    {"description_wrong_indent", R"FLOW6(---
description:
   Flow.
---
flowchart LR
A --> B
)FLOW6",
     "eb0b18f280f7ac029c5bf14f5bd34a20c1454bc06189aeb6e5a841877bf82f8c"},
    {"description_mapping", R"FLOW6(---
description:
  key: value
---
flowchart LR
A --> B
)FLOW6",
     "319b60581b24d954de75a94d1be0d93a6b06b4fff082cb9867515fdbc704f375"},
    {"description_comment", R"FLOW6(---
description:
  Flow # truncated
---
flowchart LR
A --> B
)FLOW6",
     "700d5cb1bb31c7552d95c7ccae30d311de0e1df085da180015c8a0cd6b19a9db"},
    {"empty_description", R"FLOW6(---
description:
---
flowchart LR
A --> B
)FLOW6",
     "6a95c2a9c797fcb8127e5adfe8d491698c89afc74331dacec78b206c6211ffb4"},
    {"missing_close", R"FLOW6(---
description:
  Flow.
flowchart LR
A --> B
)FLOW6",
     "5dcddee91fb357c62bbad83ddada550ea13c7833307b45cc8105bafb11a47ad4"},
    {"close_has_tail", R"FLOW6(---
description:
  Flow.
--- tail
flowchart LR
A --> B
)FLOW6",
     "0b16ebd7947da96a54d1646160952f421d6050f659e41fee6bb5cdd2282e9d11"},
    {"directive_before", R"FLOW6(%%{init: {"layout":"dagre"}}%%
---
description:
  Flow.
---
flowchart LR
A --> B
)FLOW6",
     "9adbdc88b039022466594d8fbeef5af00f94a76eeef7b4fb4ec634a40ee45a57"},
    {"directive_after", R"FLOW6(---
description:
  Flow.
---
%%{init: {"layout":"dagre"}}%%
flowchart LR
A --> B
)FLOW6",
     "56113065287138bd1e610728ca068bb2c261425466b5112729e1971677ff260c"},
    {"directive_in_body", R"FLOW6(---
description:
  Flow.
---
flowchart LR
%%{init: {"layout":"dagre"}}%%
A --> B
)FLOW6",
     "c946da2c7f733a5154a5b62d7d013ef9614a9e2cd10e88203f34e38c5b33d9c8"},
    {"bare_CR", "---\ndescription:\n  Flow.\n---\nflowchart LR\nA --> B\rC\n",
     "cacc68c3326fee0759de279062088635aa9ebd757371c45028dc8fe7c84fd5c2"},
    {"layout_bare_without_separator", R"FLOW6(---
config:
  layout:dagre
---
flowchart LR
A --> B
)FLOW6",
     "f8053b4f7d954ddc66f07196c2ca6b2a8b9529ff3748f1af6e05c5374180b7b8"},
    {"layout_quoted_without_separator", R"FLOW6(---
config:
  layout:"dagre"
---
flowchart LR
A --> B
)FLOW6",
     "799b2c768d9719c27c19159e088d0776099f167fa4df91d1224f51e833f5d425"},
}};

void flow6_envelope_control(const Flow6EnvelopeControl& control, bool positive) {
    OracleWork work{kFlow400SceneLimit};
    work.spend(16384u + 8u * sizeof(std::optional<Grid>) + 8u * sizeof(Model));
    work.product(control.source.size(), 32u);
    work.product(control.name.size(), 4u);
    SCOPED_TRACE(std::string(control.name));
    auto element = loom::ui::render_markdown(fenced(control.source, "mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(control.source, "text"));
    element->ComputeRequirement();
    ordinary->ComputeRequirement();
    // Both real requirements include the independent following paragraph.
    const int height = std::max(element->requirement().min_y, ordinary->requirement().min_y) + 1;
    flow400_require(height > 0 && height <= 1040, "Flow6 protocol full control height");
    std::array<std::optional<Grid>, 5> frames;
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    for (std::size_t n = 0; n < frames.size(); ++n)
        frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
    // Complete source binding and complete Model construction follow all five captures.
    work.product(control.source.size(), 4u);
    flow400_require(digest(control.source) == control.source_sha256,
                    "Flow6 protocol complete source SHA");
    for (const auto& frame : frames)
        static_cast<void>(flow400_following(*frame, work));
    const bool source_wide = flow400_same(*frames[0], *frames[3], work);
    const bool source_narrow = flow400_same(*frames[1], *frames[4], work);
    flow400_require(flow400_same(*frames[0], *frames[2], work),
                    "Flow6 protocol restored control differs");
    if (!positive) {
        flow400_require(source_wide && source_narrow,
                        "Flow6 unsupported whole envelope unexpectedly drawn");
        return;
    }
    work.spend(8u * sizeof(Model) + 8u * sizeof(Node) + 8u * sizeof(Edge) + 4096u);
    const Model model{"LR", {{"A", "A"}, {"B", "B"}}, {{0, 1, "", "solid", false, true}}, {}};
    flow400_require(!source_wide, "Flow6 mandatory whole envelope returned source");
    const auto errors = flow400_geometry(*frames[0], model, work);
    flow400_require(errors.empty(), "Flow6 complete whole positive geometry failed");
    if (!source_narrow) {
        const auto narrow_errors = flow400_geometry(*frames[1], model, work);
        flow400_require(narrow_errors.empty(), "Flow6 complete narrow positive geometry failed");
    }
    const auto rejected = flow400_mutations(*frames[0], model, work);
    flow400_require(rejected >= 3u, "Flow6 missing meaningful complete Model/physical mutants");
}

TEST(MermaidFlowEnvelope, WholeSourcePlainDescriptionsAndSingletonDagre) {
    OracleWork setup;
    setup.spend(4096u + 16u * sizeof(std::string));
    RecordProperty("flow6_control_scene_limit", "128000000");
    RecordProperty("flow6_control_sizeof_Pixel", std::to_string(sizeof(ftxui::Pixel)));
    RecordProperty("flow6_control_sizeof_string", std::to_string(sizeof(std::string)));
    for (const auto& control : kFlow6EnvelopePositive) {
        try {
            flow6_envelope_control(control, true);
        } catch (const std::exception& error) {
            ADD_FAILURE() << control.name << ": " << error.what();
        }
    }
}

TEST(MermaidFlowEnvelope, UnknownOrIncompleteWholeEnvelopesStayClosed) {
    for (const auto& control : kFlow6EnvelopeClosed) {
        try {
            flow6_envelope_control(control, false);
        } catch (const std::exception& error) {
            // Resource/protocol failure is a failing TEST, never closed-profile credit.
            ADD_FAILURE() << control.name << ": " << error.what();
        }
    }
}
}  // namespace

namespace {
TEST(MermaidFlowOracle, StrictAlternateFollowingWholeScenesAndRestoredDamage) {
    constexpr std::string_view token = "AFTER_MERMAID";
    constexpr std::string_view source = "flowchart LR\nBusiness[\"AFTER_MERMAID\"]\n";
    constexpr std::string_view source_sha =
        "d976fd133de62e58dce8a7f47cef95f93f3be9ebce81d412831b2b7cefe2e8f7";
    OracleWork paper;
    paper.spend(32768u + 8u * sizeof(Grid) + 8u * sizeof(Model) + 32u * sizeof(std::string));
    const Model meaning{"LR", {{"Business", std::string(token)}}, {}, {}};
    const auto good =
        mock({"┌───────────────┐", "│ AFTER_MERMAID │", "└───────────────┘", "", token, ""});
    ASSERT_TRUE(flow400_geometry(good, meaning, paper, token).empty());
    ASSERT_GE(flow400_mutations(good, meaning, paper, token), 2u);
    auto physical_reject = [&](const Grid& bad) {
        bool rejected = false;
        try {
            const auto errors = flow400_geometry(bad, meaning, paper, token);
            rejected = !errors.empty();
            for (const auto& error : errors)
                flow400_require(!flow400_resource(error), "Stage2A resource not physical credit");
        } catch (const std::exception& error) {
            flow400_require(std::string_view(error.what()) ==
                                "Flow400 protocol exact independent following row",
                            "Stage2A only exact physical footer rejection qualifies");
            rejected = true;
        }
        flow400_require(rejected && paper.remaining > 0u,
                        "Stage2A physical mutant not meaningfully rejected");
    };
    // Real business token stays present while the independent footer is lost.
    flow400_copy_grid(good, paper);
    auto missing = good;
    paper.spend(32u);
    missing.put({0, 4}, "X");
    physical_reject(missing);
    flow400_copy_grid(good, paper);
    auto business = good;
    paper.spend(32u);
    business.put({2, 1}, "X");
    physical_reject(business);
    flow400_copy_grid(good, paper);
    auto packed = good;
    paper.spend(1024u);
    for (int x = 0; x < packed.width; ++x)
        packed.put({x, 4}, " ");
    packed.put({0, 4}, std::string(token));
    physical_reject(packed);
    flow400_copy_grid(good, paper);
    auto duplicate = good;
    paper.spend(1024u);
    for (std::size_t x = 0; x < token.size(); ++x)
        duplicate.put({static_cast<int>(x), 3}, std::string(token.substr(x, 1u)));
    physical_reject(duplicate);
    // A complete wide scene cannot certify a separately corrupted restored one.
    flow400_copy_grid(good, paper);
    auto restored_bad = good;
    paper.spend(32u);
    restored_bad.put({0, 4}, "X");
    ASSERT_TRUE(flow400_geometry(good, meaning, paper, token).empty());
    EXPECT_FALSE(flow400_same(good, restored_bad, paper));
    physical_reject(restored_bad);
    // Original Native token has its exact pre-parameterization numeric tariff.
    paper.spend(16384u);
    const auto legacy = mock({"┌───┐", "│ A │", "└───┘", "", kSentinel, ""});
    paper.product(legacy.cells.size(), 8u);
    std::size_t payload = 0;
    for (const auto& cell : legacy.cells)
        payload += cell.size();
    const auto expected = 16u * sizeof(std::size_t) + 256u +
                          legacy.cells.size() * (2u * sizeof(std::string) + 12u) + 2u * payload;
    const auto before = paper.remaining;
    static_cast<void>(flow400_following(legacy, paper));
    EXPECT_EQ(before - paper.remaining, expected);
    // All five real public views precede this source's binding/Model allocation.
    OracleWork scene{kFlow400SceneLimit};
    scene.spend(32768u + 8u * sizeof(std::optional<Grid>) + 8u * sizeof(Model));
    scene.product(source.size(), 32u);
    scene.product(token.size(), 8u);
    const auto fence = [&](std::string_view language) {
        return "```" + std::string(language) + "\n" + std::string(source) + "\n```\n\n" +
               std::string(token);
    };
    auto element = loom::ui::render_markdown(fence("mermaid"));
    auto ordinary = loom::ui::render_markdown(fence("text"));
    element->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height =
        std::max({1, element->requirement().min_y, ordinary->requirement().min_y}) + 1;
    flow400_require(height <= 1040, "Stage2A full public height");
    std::array<std::optional<Grid>, 5> frames;
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    for (std::size_t n = 0; n < frames.size(); ++n)
        frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &scene);
    scene.product(source.size(), 4u);
    flow400_require(digest(source) == source_sha, "Stage2A complete source binding");
    scene.spend(8u * sizeof(Model) + 8u * sizeof(Node) + 4096u);
    const Model actual{"LR", {{"Business", std::string(token)}}, {}, {}};
    for (const auto& frame : frames)
        static_cast<void>(flow400_following(*frame, scene, token));
    ASSERT_FALSE(flow400_same(*frames[0], *frames[3], scene));
    ASSERT_TRUE(flow400_geometry(*frames[0], actual, scene, token).empty());
    if (!flow400_same(*frames[1], *frames[4], scene))
        ASSERT_TRUE(flow400_geometry(*frames[1], actual, scene, token).empty());
    ASSERT_TRUE(flow400_same(*frames[0], *frames[2], scene));
    ASSERT_TRUE(flow400_geometry(*frames[2], actual, scene, token).empty());
    ASSERT_GE(flow400_mutations(*frames[0], actual, scene, token), 2u);
}
}  // namespace

namespace {
// Full source-authored repeated-group Models; no captured frame selects one.
json::JsonDoc flow_repeat_documents(OracleWork& setup) {
    setup.spend(4u * sizeof(json::JsonDoc) + 8192u);
    setup.product(std::string_view{LOOM_TESTS_DIR}.size(), 4u);
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/flow_repeated_subgraph2_test_first.v1.json";
    const auto bytes = std::filesystem::file_size(path);
    flow400_require(bytes == 7807u, "Flow repeat protocol fixed asset bytes");
    setup.product(static_cast<std::size_t>(bytes), 3u);
    const auto text = flow400_read(path);
    flow400_require(digest(text) ==
                        "f800f05b590d1f2b66743b780af9742afdf83e441ae2f68ba7b56ce9c041ed18",
                    "Flow repeat protocol fixed asset SHA");
    auto document = json::parse(text);
    flow400_require(document.has_value(), "Flow repeat protocol asset JSON");
    const auto root = document->root();
    flow400_keys(root, {"schema", "source_gate", "positive", "closed", "cases"});
    flow400_require(flow400_text(root.get("schema")) == "flow_repeated_subgraph2_test_first/v1" &&
                        flow400_text(root.get("source_gate")) ==
                            "2b9461a565a46efaf45ef172bf0c9c875f94c681477284953dafe25482785179" &&
                        flow400_number(root.get("positive")) == 7u &&
                        flow400_number(root.get("closed")) == 10u,
                    "Flow repeat protocol complete inventory");
    const auto cases = root.get("cases");
    flow400_require(cases.is_arr() && cases.size() == 17u, "Flow repeat protocol case count");
    return std::move(*document);
}

std::size_t flow_repeat_mutations(const Grid& good, const Model& model, OracleWork& work) {
    std::size_t rejected = flow400_mutations(good, model, work);
    const auto reject = [&](const Model& wrong) {
        const auto errors = flow400_geometry(good, wrong, work);
        flow400_require(!errors.empty(), "Flow repeat semantic Model mutant accepted");
        for (const auto& error : errors)
            flow400_require(!flow400_resource(error),
                            "Flow repeat resource is not semantic-negative credit");
        ++rejected;
    };
    flow400_require(!model.groups.empty(), "Flow repeat protocol missing complete Group Model");
    flow400_copy_model(model, work);
    auto title = model;
    work.spend(1024u);
    title.groups.front().label += "__absent_repeat_group";
    reject(title);
    if (!model.edges.empty()) {
        // A complete route remains physically drawn; removing one business edge
        // must not be hidden by the group's membership union.
        flow400_copy_model(model, work);
        auto edge = model;
        work.product(edge.edges.size(), 2u * sizeof(Edge) + 8u);
        edge.edges.erase(edge.edges.begin());
        reject(edge);
    }
    if (model.groups.size() > 1u && model.groups.back().parent >= 0) {
        flow400_copy_model(model, work);
        auto parent = model;
        parent.groups.back().parent = -1;
        reject(parent);
        flow400_copy_model(model, work);
        auto merge = model;
        const auto from = merge.groups.size() - 1u;
        const auto into = static_cast<std::size_t>(merge.groups[from].parent);
        work.spend(64u);
        work.product(merge.groups[into].nodes.size() + merge.groups[from].nodes.size(),
                     4u * sizeof(int) + 8u);
        merge.groups[into].nodes.insert(merge.groups[into].nodes.end(),
                                        merge.groups[from].nodes.begin(),
                                        merge.groups[from].nodes.end());
        merge.groups.pop_back();
        reject(merge);
    }
    return rejected;
}

void flow_repeat_control(JsonVal row, bool positive) {
    // Predeclared existing 128M whole-scene measurement cap; never reset.
    OracleWork work{kFlow400SceneLimit};
    work.spend(16384u + 8u * sizeof(std::optional<Grid>) + 8u * sizeof(Model));
    flow400_keys(row,
                 {"name", "source", "source_sha256", "model_json", "model_sha256", "adaptation"});
    const auto name = flow400_text(row.get("name"));
    const auto source = flow400_text(row.get("source"));
    flow400_require(name.size() <= 80u && source.size() <= 1024u,
                    "Flow repeat protocol bounded complete source");
    work.product(name.size(), 4u);
    work.product(source.size(), 32u);
    SCOPED_TRACE(std::string(name));
    auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
    element->ComputeRequirement();
    ordinary->ComputeRequirement();
    const int height = std::max(element->requirement().min_y, ordinary->requirement().min_y) + 1;
    flow400_require(height > 0 && height <= 1040, "Flow repeat protocol full source height");
    std::array<std::optional<Grid>, 5> frames;
    constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
    for (std::size_t n = 0; n < frames.size(); ++n)
        frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
    // All five full public frames exist before this source's Model is decoded.
    work.product(source.size(), 4u);
    flow400_require(digest(source) == flow400_text(row.get("source_sha256")),
                    "Flow repeat protocol complete source SHA");
    const auto model = flow400_model(row, work);
    flow400_require(model.has_value() == positive, "Flow repeat protocol whole Model role");
    for (const auto& frame : frames)
        static_cast<void>(flow400_following(*frame, work));
    const bool source_wide = flow400_same(*frames[0], *frames[3], work);
    const bool source_narrow = flow400_same(*frames[1], *frames[4], work);
    flow400_require(flow400_same(*frames[0], *frames[2], work),
                    "Flow repeat protocol restored whole frame differs");
    if (!positive) {
        flow400_require(source_wide && source_narrow,
                        "Flow repeat unsupported whole profile unexpectedly drawn");
        return;
    }
    flow400_require(!source_wide, "Flow repeat mandatory complete source returned source");
    const auto wide_errors = flow400_geometry(*frames[0], *model, work);
    flow400_require(wide_errors.empty(), "Flow repeat complete wide geometry failed");
    if (!source_narrow) {
        const auto narrow_errors = flow400_geometry(*frames[1], *model, work);
        flow400_require(narrow_errors.empty(), "Flow repeat complete narrow geometry failed");
    }
    const auto restored_errors = flow400_geometry(*frames[2], *model, work);
    flow400_require(restored_errors.empty(), "Flow repeat complete restored geometry failed");
    flow400_require(flow_repeat_mutations(*frames[0], *model, work) >= 4u,
                    "Flow repeat missing meaningful complete Model/physical mutants");
}

TEST(MermaidFlowRepeatedSubgraph, CompleteOriginalsAndStableMembershipControls) {
    OracleWork setup;
    auto document = flow_repeat_documents(setup);
    std::size_t observed = 0;
    document.root().get("cases").iter([&](JsonVal row) {
        setup.spend(1024u);
        if (row.get("model_json").is_null()) return;
        ++observed;
        try {
            flow_repeat_control(row, true);
        } catch (const std::exception& error) {
            // Publication is bounded and already reserved by the scene's 16K.
            const char* text = error.what();
            std::size_t length = 0;
            while (length < 256u && text[length])
                ++length;
            ADD_FAILURE() << flow400_text(row.get("name")) << ": "
                          << std::string_view(text, length);
        }
    });
    EXPECT_EQ(observed, 7u);
}

TEST(MermaidFlowRepeatedSubgraph, WholeSourceDirectionOwnershipAndIdentityBoundaries) {
    OracleWork setup;
    auto document = flow_repeat_documents(setup);
    std::size_t observed = 0;
    document.root().get("cases").iter([&](JsonVal row) {
        setup.spend(1024u);
        if (!row.get("model_json").is_null()) return;
        ++observed;
        try {
            flow_repeat_control(row, false);
        } catch (const std::exception& error) {
            const char* text = error.what();
            std::size_t length = 0;
            while (length < 256u && text[length])
                ++length;
            ADD_FAILURE() << flow400_text(row.get("name")) << ": "
                          << std::string_view(text, length);
        }
    });
    EXPECT_EQ(observed, 10u);
}
}  // namespace

// Pure EOF append for tests/ui/test_mermaid_flow_semantics.cpp.
#include "mermaid_flow400_typed_source4_authority.hpp"

namespace {
using namespace loom::test::mermaid_flow;

struct Flow400Config21Observation {
    std::array<std::optional<Grid>, 5> frames;
    std::array<std::size_t, 8> remaining{};
    std::array<char, 257> error{}, wide_errors{}, narrow_errors{}, restored_errors{};
    std::size_t captures = 0, published = 0, rejected = 0, required_rejections = 0;
    bool source_bound = false, model_decoded = false, source_wide = false, source_narrow = false;
    bool wide_geometry = false, narrow_geometry = false, restored = false, admitted = false,
         resource = false;
};

std::size_t flow400_config21_close_array(std::string_view text, std::size_t open) {
    flow400_require(open < text.size() && text[open] == '[', "Flow400 Config21 test array open");
    bool quoted = false, escaped = false;
    std::size_t depth = 0;
    for (std::size_t at = open; at < text.size(); ++at) {
        const char c = text[at];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (quoted && c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            quoted = !quoted;
            continue;
        }
        if (quoted) continue;
        if (c == '[')
            ++depth;
        else if (c == ']' && --depth == 0u)
            return at;
    }
    throw std::runtime_error("Flow400 Config21 test array close");
}

void flow400_config21_expect_failure(auto&& action, std::string_view expected) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception& error) {
        rejected = error.what() == expected;
    }
    EXPECT_TRUE(rejected) << expected;
}

TEST(MermaidFlowConfig21Authority, RejectsTamperedSourceConfigModelAndAdmission) {
    auto supplement = flow400_config21_documents();
    auto originals = flow400_documents();
    const auto root = supplement.supplement.root(), cases = root.get("cases"), row = cases.at(0),
               authority = row.get("authority");
    const auto fixtures = originals.manifest.root().get("fixtures");
    const auto global = flow400_number(authority.get("manifest_global_ordinal"));
    const auto fixture = fixtures.at(global);
    const auto path = flow400_text(fixture.get("path"));
    const auto source = flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                                     "fixtures/mermaid_official/cases" / std::string(path));
    ASSERT_EQ(flow400_text(authority.get("fixture")), path);

    auto duplicate_text = root.to_string();
    const auto cases_open = duplicate_text.find("\"cases\":[");
    ASSERT_NE(cases_open, std::string::npos);
    const auto close = flow400_config21_close_array(duplicate_text, cases_open + 8u);
    const auto complete_cases =
        duplicate_text.substr(cases_open + 8u, close - (cases_open + 8u) + 1u);
    duplicate_text.insert(close + 1u, ",\"cases\":" + complete_cases);
    auto duplicate = json::parse(duplicate_text);
    ASSERT_TRUE(duplicate.has_value());
    OracleWork duplicate_work{kFlow400SceneLimit};
    flow400_config21_expect_failure(
        [&] { flow400_config21_root(duplicate->root(), duplicate_work); },
        "Flow400 Config21 root duplicate key");

    for (const auto key : {"independent_checks_raw_sha256", "independent_review_raw_sha256"}) {
        auto changed_lock = root.get("source_model_proposal_lock").to_string();
        const auto expected = flow400_text(root.get("source_model_proposal_lock").get(key));
        const auto at = changed_lock.find(expected);
        ASSERT_NE(at, std::string::npos);
        changed_lock.replace(at, expected.size(), std::string(64u, '0'));
        auto changed_root = root.to_string();
        const auto original_lock = root.get("source_model_proposal_lock").to_string();
        const auto lock_at = changed_root.find(original_lock);
        ASSERT_NE(lock_at, std::string::npos);
        changed_root.replace(lock_at, original_lock.size(), changed_lock);
        auto altered = json::parse(changed_root);
        ASSERT_TRUE(altered.has_value());
        OracleWork altered_work{kFlow400SceneLimit};
        flow400_config21_expect_failure(
            [&] { flow400_config21_root(altered->root(), altered_work); },
            "Flow400 Config21 source-Model lock");
    }

    auto unknown_text = row.to_string();
    const auto metadata = unknown_text.find("\"metadata\":");
    ASSERT_NE(metadata, std::string::npos);
    unknown_text.replace(metadata, 11u, "\"unknown\":");
    auto unknown = json::parse(unknown_text);
    ASSERT_TRUE(unknown.has_value());
    OracleWork unknown_work{kFlow400SceneLimit};
    flow400_config21_expect_failure(
        [&] { flow400_config21_case_shape(unknown->root(), unknown_work); },
        "Flow400 Config21 case unknown key");

    auto missing = json::parse("{\"authority\":" + authority.to_string() + "}");
    ASSERT_TRUE(missing.has_value());
    OracleWork missing_work{kFlow400SceneLimit};
    flow400_config21_expect_failure(
        [&] { flow400_config21_case_shape(missing->root(), missing_work); },
        "Flow400 Config21 case missing key");

    auto config_text = row.to_string();
    const auto dagre = config_text.find("\"dagre\"");
    ASSERT_NE(dagre, std::string::npos);
    config_text.replace(dagre, 7u, "\"xagre\"");
    auto bad_config = json::parse(config_text);
    ASSERT_TRUE(bad_config.has_value());
    OracleWork config_work{kFlow400SceneLimit};
    flow400_config21_expect_failure(
        [&] { flow400_config21_case_shape(bad_config->root(), config_work); },
        "Flow400 Config21 config SHA");

    OracleWork source_work{kFlow400SceneLimit};
    flow400_config21_expect_failure(
        [&] {
            flow400_config21_bind(row, fixture, flow400_number(authority.get("ordinal")), global,
                                  source + "X", source_work);
        },
        "Flow400 protocol complete source bytes");

    auto model_text = authority.to_string();
    const auto model_hash = flow400_text(authority.get("model_sha256"));
    const auto hash_at = model_text.find(model_hash);
    ASSERT_NE(hash_at, std::string::npos);
    model_text.replace(hash_at, model_hash.size(), std::string(64u, '0'));
    auto bad_model = json::parse(model_text);
    ASSERT_TRUE(bad_model.has_value());
    OracleWork model_work{kFlow400SceneLimit};
    flow400_config21_expect_failure(
        [&] { static_cast<void>(flow400_model(bad_model->root(), model_work)); },
        "Flow400 protocol Model text SHA");

    auto wrong_kind_text = authority.to_string();
    const auto key = wrong_kind_text.find("\"model_json\":");
    ASSERT_NE(key, std::string::npos);
    auto first = key + 13u;
    ASSERT_LT(first, wrong_kind_text.size());
    ASSERT_EQ(wrong_kind_text[first], '"');
    bool escaped = false;
    std::size_t last = first + 1u;
    for (; last < wrong_kind_text.size(); ++last) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (wrong_kind_text[last] == '\\') {
            escaped = true;
            continue;
        }
        if (wrong_kind_text[last] == '"') break;
    }
    ASSERT_LT(last, wrong_kind_text.size());
    wrong_kind_text.replace(first, last - first + 1u, "0");
    auto wrong_kind = json::parse(wrong_kind_text);
    ASSERT_TRUE(wrong_kind.has_value());
    OracleWork kind_work{kFlow400SceneLimit};
    flow400_config21_expect_failure(
        [&] { static_cast<void>(flow400_model(wrong_kind->root(), kind_work)); },
        "Flow400 protocol expected string");

    OracleWork zero{0u};
    flow400_config21_expect_failure([&] { flow400_config21_case_shape(row, zero); },
                                    "Bounded cell-oracle work exhausted");
    EXPECT_FALSE(flow400_config21_admission_allowed(true, true, true, true, 3u, 3u));
    EXPECT_FALSE(flow400_config21_admission_allowed(false, true, true, true, 2u, 3u));
    EXPECT_FALSE(flow400_config21_admission_allowed(false, true, true, true, 3u, 2u));
    EXPECT_TRUE(flow400_config21_admission_allowed(false, true, true, true, 2u, 2u));
}

TEST(MermaidFlowConfig21Native, All21CompleteSourcesRequireNativeWholeSemantics) {
    auto supplement = flow400_config21_documents();
    auto originals = flow400_documents();
    const auto root = supplement.supplement.root(), cases = root.get("cases"),
               fixtures = originals.manifest.root().get("fixtures");
    json::JsonMutDoc report;
    auto report_root = report.object(), report_cases = report_root.make_arr();
    report_root.set("kind", "flow400_config21_native_whole_source_test_first");
    report_root.set("qualification", "All 21 approved source-derived Models require native "
                                     "whole-source semantics; foreign certification remains zero.");
    report_root.set("supplement_sha256", kFlow400Config21AssetSha);
    report_root.set("scene_limit", static_cast<int64_t>(kFlow400SceneLimit));
    report_root.set("old_authority_complete_models", 8);
    report_root.set("old_authority_pending", 392);
    report_root.set("foreign_certified", 0);
    std::size_t visited = 0, decoded = 0, positive = 0, mutated = 0, admitted = 0, errors = 0,
                resource_errors = 0;
    cases.iter([&](JsonVal selected) {
        const auto authority = selected.get("authority");
        const auto ordinal = flow400_number(authority.get("ordinal"));
        const auto global = flow400_number(authority.get("manifest_global_ordinal"));
        const auto fixture = fixtures.at(global);
        const auto path = flow400_text(fixture.get("path"));
        Flow400Config21Observation observation;
        OracleWork work{kFlow400SceneLimit};
        work.spend(65536u + 4u * sizeof(Flow400Config21Observation) +
                   64u * sizeof(json::JsonMutVal) + 16u * sizeof(std::string));
        auto row = report_root.make_obj();
        row.set("ordinal", static_cast<int64_t>(ordinal));
        row.set("fixture", path);
        row.set("manifest_global_ordinal", static_cast<int64_t>(global));
        row.set("source_contract", flow400_text(selected.get("source_contract")));
        row.set("source_sha256", flow400_text(authority.get("source_sha256")));
        row.set("source_git_blob", flow400_text(authority.get("source_git_blob")));
        row.set("model_sha256", flow400_text(authority.get("model_sha256")));
        row.set("oracle_record_sha256", flow400_text(authority.get("oracle_record_sha256")));
        const char* phase = "source";
        try {
            const auto source = flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                                             "fixtures/mermaid_official/cases" / std::string(path));
            work.product(source.size(), 32u);
            work.product(path.size(), 8u);
            work.spend(16384u);
            flow400_require(source.size() == flow400_number(authority.get("source_bytes")),
                            "Flow400 Config21 complete source bytes");
            SCOPED_TRACE(std::string(path));
            auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
            element->ComputeRequirement();
            ordinary->ComputeRequirement();
            const auto line_floor =
                static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 12;
            const int height =
                std::min(1040, std::max({32, element->requirement().min_y + 8,
                                         ordinary->requirement().min_y + 8, line_floor}));
            flow400_require(height > 0, "Flow400 Config21 capture height");
            row.set("height", height);
            phase = "capture";
            constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            for (std::size_t n = 0; n < 5u; ++n) {
                observation.frames[n] =
                    capture(n < 3u ? element : ordinary, widths[n], height, &work);
                ++observation.captures;
                phase = "publication";
                flow400_publication(*observation.frames[n], work);
                ++observation.published;
                phase = "capture";
            }
            flow400_require(observation.captures == 5u && observation.published == 5u,
                            "Flow400 Config21 full frames not published");
            observation.remaining[0] = work.remaining;
            phase = "following";
            for (const auto& frame : observation.frames)
                static_cast<void>(flow400_following(*frame, work));
            observation.source_wide =
                flow400_same(*observation.frames[0], *observation.frames[3], work);
            observation.source_narrow =
                flow400_same(*observation.frames[1], *observation.frames[4], work);
            observation.restored =
                flow400_same(*observation.frames[0], *observation.frames[2], work);
            observation.remaining[1] = work.remaining;
            phase = "bind";
            flow400_config21_bind(selected, fixture, ordinal, global, source, work);
            observation.source_bound = true;
            observation.remaining[2] = work.remaining;
            phase = "model";
            const auto model = flow400_model(authority, work);
            flow400_require(model.has_value(), "Flow400 Config21 mandatory Model absent");
            observation.model_decoded = true;
            ++decoded;
            observation.required_rejections =
                2u +
                ((!model->edges.empty() && model->edges.front().from != model->edges.front().to)
                     ? 1u
                     : 0u) +
                (std::ranges::any_of(model->groups,
                                     [](const Group& group) { return !group.nodes.empty(); })
                     ? 1u
                     : 0u);
            phase = "ordinary";
            flow400_require(!observation.source_wide,
                            "Flow400 Config21 mandatory positive returned ordinary source");
            phase = "wide_geometry";
            const auto wide_errors = flow400_geometry(*observation.frames[0], *model, work);
            if (!wide_errors.empty()) {
                const auto diagnostic = std::string_view(wide_errors.front()).substr(0u, 256u);
                std::copy_n(diagnostic.begin(), diagnostic.size(), observation.wide_errors.begin());
                throw std::runtime_error(std::string(diagnostic));
            }
            observation.wide_geometry = true;
            phase = "narrow_geometry";
            if (observation.source_narrow)
                observation.narrow_geometry = true;
            else {
                const auto narrow_errors = flow400_geometry(*observation.frames[1], *model, work);
                if (!narrow_errors.empty()) {
                    const auto diagnostic =
                        std::string_view(narrow_errors.front()).substr(0u, 256u);
                    std::copy_n(diagnostic.begin(), diagnostic.size(),
                                observation.narrow_errors.begin());
                    throw std::runtime_error(std::string(diagnostic));
                }
                observation.narrow_geometry = true;
            }
            phase = "restoration";
            flow400_require(observation.restored, "Flow400 Config21 restored frame differs");
            const auto restored_errors = flow400_geometry(*observation.frames[2], *model, work);
            if (!restored_errors.empty()) {
                const auto diagnostic = std::string_view(restored_errors.front()).substr(0u, 256u);
                std::copy_n(diagnostic.begin(), diagnostic.size(),
                            observation.restored_errors.begin());
                throw std::runtime_error(std::string(diagnostic));
            }
            ++positive;
            observation.remaining[3] = work.remaining;
            phase = "mutations";
            observation.rejected = flow400_mutations(*observation.frames[0], *model, work);
            flow400_require(observation.rejected == observation.required_rejections,
                            "Flow400 Config21 missing applicable good-first mutations");
            ++mutated;
            observation.remaining[4] = work.remaining;
            phase = "admission";
            observation.admitted = flow400_config21_admission_allowed(
                observation.source_wide, observation.wide_geometry, observation.narrow_geometry,
                observation.restored, observation.rejected, observation.required_rejections);
            flow400_require(observation.admitted,
                            "Flow400 Config21 forged or incomplete admission");
            ++admitted;
            observation.remaining[5] = work.remaining;
        } catch (const std::exception& error) {
            std::size_t length = 0;
            for (; length < 256u && error.what()[length]; ++length)
                observation.error[length] = error.what()[length];
            const auto message = std::string_view(observation.error.data(), length);
            observation.resource = work.remaining == 0u || flow400_resource(message);
            ++errors;
            if (observation.resource) ++resource_errors;
        }
        row.set("phase", phase);
        row.set("error_class", observation.resource ? "resource" : phase);
        row.set("error", std::string_view(observation.error.data()));
        row.set("resource_error", observation.resource);
        row.set("capture_count", static_cast<int64_t>(observation.captures));
        row.set("published_frames", static_cast<int64_t>(observation.published));
        row.set("source_bound", observation.source_bound);
        row.set("expectation_decoded", observation.model_decoded);
        row.set("source_wide", observation.source_wide);
        row.set("source_narrow", observation.source_narrow);
        row.set("wide_geometry", observation.wide_geometry);
        row.set("narrow_geometry", observation.narrow_geometry);
        row.set("restored_identically", observation.restored);
        row.set("semantic_mutations_required",
                static_cast<int64_t>(observation.required_rejections));
        row.set("semantic_mutations_rejected", static_cast<int64_t>(observation.rejected));
        row.set("wide_errors", std::string_view(observation.wide_errors.data()));
        row.set("narrow_errors", std::string_view(observation.narrow_errors.data()));
        row.set("restored_errors", std::string_view(observation.restored_errors.data()));
        row.set("whole_source_admitted", observation.admitted);
        row.set("remaining", static_cast<int64_t>(work.remaining));
        auto stages = row.make_arr();
        for (const auto remaining : observation.remaining)
            stages.append(row.make_num(static_cast<int64_t>(remaining)));
        row.add("phase_remaining", stages);
        constexpr std::array<std::string_view, 5> names{{"wide_frame", "narrow_frame",
                                                         "restored_frame", "ordinary_wide_frame",
                                                         "ordinary_narrow_frame"}};
        for (std::size_t n = 0; n < observation.published; ++n)
            flow400_append_frame(row, names[n], *observation.frames[n]);
        report_cases.append(row);
        ++visited;
    });
    report_root.set("visited", static_cast<int64_t>(visited));
    report_root.set("complete_models_decoded", static_cast<int64_t>(decoded));
    report_root.set("complete_positive_geometry", static_cast<int64_t>(positive));
    report_root.set("mutated_positive", static_cast<int64_t>(mutated));
    report_root.set("whole_source_admitted", static_cast<int64_t>(admitted));
    report_root.set("errors", static_cast<int64_t>(errors));
    report_root.set("resource_errors", static_cast<int64_t>(resource_errors));
    report_root.add("cases", report_cases);
    report.set_root(report_root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto target =
        std::filesystem::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-flow-config21-native.json";
    std::ofstream output(target, std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n')) << "Cannot publish " << target;
#endif
    EXPECT_EQ(visited, 21u);
    EXPECT_EQ(decoded, 21u);
    EXPECT_EQ(positive, 21u);
    EXPECT_EQ(mutated, 21u);
    EXPECT_EQ(admitted, 21u);
    EXPECT_EQ(errors, 0u);
    EXPECT_EQ(resource_errors, 0u);
}
}  // namespace

namespace {
constexpr std::string_view kFlowConfigControlAssetSha =
    "3d76c2769eda94b06350bab2e7ee811faf7d03ed2a75a22ea668cb93850f6740";

json::JsonDoc flow_config_control_documents() {
    OracleWork setup;
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/flow400_config_source_controls.v1.json";
    flow400_require(std::filesystem::file_size(path) == 13660u, "Flow config controls asset size");
    setup.spend(16384u);
    setup.product(13660u, 12u);
    const auto bytes = flow400_read(path);
    flow400_require(digest(bytes) == kFlowConfigControlAssetSha, "Flow config controls asset SHA");
    auto doc = json::parse(bytes);
    flow400_require(doc.has_value(), "Flow config controls JSON");
    const auto root = doc->root();
    flow400_keys(root, {"schema", "qualification", "source_contract_sha256", "counts", "cases"});
    flow400_require(flow400_text(root.get("schema")) == "flow400_config_source_controls/v1",
                    "Flow config controls schema");
    const auto counts = root.get("counts");
    flow400_keys(counts, {"all", "positive", "closed"});
    flow400_require(flow400_number(counts.get("all")) == 23u &&
                        flow400_number(counts.get("positive")) == 10u &&
                        flow400_number(counts.get("closed")) == 13u,
                    "Flow config controls denominator");
    const auto rows = root.get("cases");
    flow400_require(rows.is_arr() && rows.size() == 23u, "Flow config controls inventory");
    std::set<std::string> names;
    std::size_t positives = 0u, closed = 0u;
    rows.iter([&](JsonVal row) {
        setup.spend(2048u);
        flow400_keys(row, {"name", "source", "source_sha256", "model_json", "model_sha256",
                           "adaptation", "positive"});
        const auto name = flow400_text(row.get("name"));
        flow400_require(name.size() <= 80u && names.insert(std::string(name)).second,
                        "Flow config controls identity");
        flow400_require(row.get("positive").is_bool(), "Flow config controls polarity");
        if (row.get("positive").as_bool())
            ++positives;
        else
            ++closed;
    });
    flow400_require(positives == 10u && closed == 13u,
                    "Flow config controls complete polarity counts");
    return std::move(*doc);
}

json::JsonDoc flow_folded_shape_data_documents();

void flow_config_source_controls(bool positive) {
    auto controls = flow_config_control_documents();
    const auto folded = flow_folded_shape_data_documents();
    const auto promoted = folded.root().get("controls").at(0u);
    flow400_require(flow400_text(promoted.get("id")) == "folded_description_legacy_control",
                    "Flow config folded promotion identity");
    json::JsonMutDoc report;
    auto root = report.object(), rows = root.make_arr();
    root.set("kind", "flow_config_source_controls/v1");
    root.set("positive_controls", positive);
    root.set("qualification", "Synthetic controls only; no original corpus or foreign assertion "
                              "credit. Closed source fallback is not positive admission.");
    root.set("asset_sha256", kFlowConfigControlAssetSha);
    std::size_t visited = 0u, passed = 0u, failed = 0u, resources = 0u;
    controls.root().get("cases").iter([&](JsonVal item) {
        const bool promotion = flow400_text(item.get("name")) == "N13";
        const bool effective_positive = item.get("positive").as_bool() || promotion;
        if (effective_positive != positive) return;
        OracleWork work{kFlow400SceneLimit};
        work.spend(65536u + 8u * sizeof(Grid) + 8u * sizeof(Model) +
                   64u * sizeof(json::JsonMutVal));
        const auto name = flow400_text(item.get("name")), source = flow400_text(item.get("source"));
        flow400_require(source.size() <= 1024u, "Flow config controls source bound");
        work.product(source.size(), 32u);
        work.product(name.size(), 8u);
        auto row = root.make_obj();
        row.set("name", name);
        row.set("positive", positive);
        row.set("source_sha256", flow400_text(item.get("source_sha256")));
        if (positive)
            row.set("expected_model_sha256",
                    flow400_text((promotion ? promoted : item).get("model_sha256")));
        else
            row.add("expected_model_sha256", row.make_null());
        std::array<std::optional<Grid>, 5> frames;
        std::array<char, 257> error{};
        std::size_t captures = 0u, published = 0u, rejected = 0u, required = 0u;
        bool bound = false, decoded = false, source_wide = false, source_narrow = false,
             restored = false, admitted = false, resource = false, complete = false;
        const char* phase = "capture";
        try {
            auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
            element->ComputeRequirement();
            ordinary->ComputeRequirement();
            const int height =
                std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
            flow400_require(height <= 1040, "Flow config controls complete height");
            constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            constexpr std::array<std::string_view, 5> names{
                {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
            for (std::size_t n = 0; n < frames.size(); ++n) {
                frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
                ++captures;
                flow400_publication(*frames[n], work);
                flow400_append_frame(row, names[n], *frames[n]);
                ++published;
            }
            flow400_require(captures == 5u && published == 5u,
                            "Flow config five full frames attached before bind");
            phase = "bind";
            work.product(source.size(), 4u);
            flow400_require(digest(source) == flow400_text(item.get("source_sha256")),
                            "Flow config controls complete source SHA");
            bound = true;
            for (const auto& frame : frames)
                static_cast<void>(flow400_following(*frame, work));
            source_wide = flow400_same(*frames[0], *frames[3], work);
            source_narrow = flow400_same(*frames[1], *frames[4], work);
            restored = flow400_same(*frames[0], *frames[2], work);
            flow400_require(restored, "Flow config controls restored cells differ");
            if (promotion) {
                flow400_require(!item.get("positive").as_bool() &&
                                    item.get("model_json").is_null() &&
                                    item.get("model_sha256").is_null(),
                                "Flow config historical N13 remains immutable");
                flow400_require(source == flow400_text(promoted.get("source")) &&
                                    digest(source) == flow400_text(promoted.get("source_sha256")),
                                "Flow config folded promotion complete source binding");
                row.set("historical_control", "N13");
                row.set("promotion_contract", "independent_folded_description_full_Model");
            }
            phase = "model";
            std::optional<Model> model;
            if (promotion) {
                const auto bytes = flow400_text(promoted.get("model_json"));
                work.product(bytes.size(), 32u);
                work.spend(8192u + 4u * sizeof(json::JsonDoc));
                flow400_require(digest(bytes) == flow400_text(promoted.get("model_sha256")),
                                "Flow config folded promotion Model SHA");
                auto document = json::parse(bytes);
                flow400_require(document.has_value(), "Flow config folded promotion Model JSON");
                model = flow_typed_shape_model(document->root(), work);
            } else
                model = flow400_model(item, work);
            flow400_require(model.has_value() == positive, "Flow config controls Model polarity");
            decoded = model.has_value();
            if (!positive) {
                phase = "closed";
                flow400_require(source_wide && source_narrow,
                                "Flow config closed whole source unexpectedly drawn");
                complete = true;
            } else {
                phase = "ordinary";
                flow400_require(!source_wide, "Flow config mandatory positive returned source");
                phase = "wide_geometry";
                const auto wide = flow400_geometry(*frames[0], *model, work);
                if (!wide.empty()) throw std::runtime_error(wide.front());
                phase = "narrow_geometry";
                if (!source_narrow) {
                    const auto narrow = flow400_geometry(*frames[1], *model, work);
                    if (!narrow.empty()) throw std::runtime_error(narrow.front());
                }
                phase = "restored_geometry";
                const auto restored_errors = flow400_geometry(*frames[2], *model, work);
                if (!restored_errors.empty()) throw std::runtime_error(restored_errors.front());
                phase = "mutations";
                required =
                    2u +
                    ((!model->edges.empty() && model->edges.front().from != model->edges.front().to)
                         ? 1u
                         : 0u) +
                    (std::ranges::any_of(model->groups,
                                         [](const Group& group) { return !group.nodes.empty(); })
                         ? 1u
                         : 0u);
                rejected = flow400_mutations(*frames[0], *model, work);
                flow400_require(rejected == required,
                                "Flow config exact applicable mutants missing");
                admitted = true;
                complete = true;
            }
            phase = "complete";
        } catch (const std::exception& failure) {
            std::size_t n = 0u;
            for (; n < 256u && failure.what()[n]; ++n)
                error[n] = failure.what()[n];
            resource = work.remaining == 0u || flow400_resource(std::string_view(error.data(), n));
        }
        row.set("phase", phase);
        row.set("outcome", complete ? (positive ? "whole_drawing_admitted" : "closed_guard_only")
                                    : (resource ? "resource_failure" : "failed"));
        row.set("error", std::string_view(error.data()));
        row.set("resource_error", resource);
        row.set("captures", static_cast<int64_t>(captures));
        row.set("published_frames", static_cast<int64_t>(published));
        row.set("source_bound", bound);
        row.set("whole_Model_decoded", decoded);
        row.set("source_wide", source_wide);
        row.set("source_narrow", source_narrow);
        row.set("restored_identically", restored);
        row.set("whole_source_admitted", admitted);
        row.set("required_mutations", static_cast<int64_t>(required));
        row.set("rejected_mutations", static_cast<int64_t>(rejected));
        row.set("remaining", static_cast<int64_t>(work.remaining));
        rows.append(row);
        ++visited;
        if (complete)
            ++passed;
        else
            ++failed;
        if (resource) ++resources;
    });
    root.add("cases", rows);
    root.set("visited", static_cast<int64_t>(visited));
    root.set("passed", static_cast<int64_t>(passed));
    root.set("failed", static_cast<int64_t>(failed));
    root.set("resources", static_cast<int64_t>(resources));
    report.set_root(root);
    const auto path = std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                      (positive ? "mermaid-flow-config-positive-controls.json"
                                : "mermaid-flow-config-closed-controls.json");
    std::ofstream output(path, std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n'));
    EXPECT_EQ(visited, positive ? 11u : 12u);
    EXPECT_EQ(passed, visited);
    EXPECT_EQ(failed, 0u);
    EXPECT_EQ(resources, 0u);
}

TEST(MermaidFlowConfigSourceControls, CompletePositiveSources) {
    flow_config_source_controls(true);
}

TEST(MermaidFlowConfigSourceControls, ExplicitClosedProfilesReceiveNoDrawingCredit) {
    flow_config_source_controls(false);
}
}  // namespace

namespace {
constexpr std::string_view kFlowTypedShapesAssetSha =
    "567778c653d7c797250860b6941d3f8457ab116b1217041b5a47cd4eb702b9fc";

json::JsonDoc flow_typed_shapes_documents() {
    OracleWork setup;
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/flow_native_typed_shapes_controls.v1.json";
    flow400_require(std::filesystem::file_size(path) == 12623u,
                    "Flow typed controls immutable asset bytes");
    setup.spend(32768u + 8u * sizeof(json::JsonDoc) + 16u * sizeof(JsonVal));
    setup.product(12623u, 16u);
    const auto bytes = flow400_read(path);
    flow400_require(digest(bytes) == kFlowTypedShapesAssetSha,
                    "Flow typed controls immutable asset SHA");
    auto document = json::parse(bytes);
    flow400_require(document.has_value(), "Flow typed controls JSON");
    const auto root = document->root();
    flow400_keys(root, {"schema", "qualification", "source_design_manifest_sha256", "counts",
                        "synthetic_controls", "original000_proof"});
    flow400_require(flow400_text(root.get("schema")) == "flow_native_typed_shapes_controls/v1" &&
                        flow400_text(root.get("source_design_manifest_sha256")) ==
                            "8069aef5c5a0fcf030d1b878254b61dabf70b3c15352d1403505f609d973e614",
                    "Flow typed controls schema/source-design lock");
    const auto counts = root.get("counts");
    flow400_keys(counts,
                 {"synthetic_all", "synthetic_expected_complete", "synthetic_required_first_red",
                  "original_proofs", "original_corpus_credit"});
    flow400_require(flow400_number(counts.get("synthetic_all")) == 9u &&
                        flow400_number(counts.get("synthetic_expected_complete")) == 8u &&
                        flow400_number(counts.get("synthetic_required_first_red")) == 1u &&
                        flow400_number(counts.get("original_proofs")) == 1u &&
                        flow400_number(counts.get("original_corpus_credit")) == 0u,
                    "Flow typed controls denominator/credit");
    const auto controls = root.get("synthetic_controls");
    flow400_require(controls.is_arr() && controls.size() == 9u, "Flow typed controls inventory");
    std::set<std::string> ids;
    std::size_t first_red = 0u;
    controls.iter([&](JsonVal row) {
        setup.spend(2048u);
        flow400_keys(row, {"id", "provenance", "source", "source_bytes", "source_sha256",
                           "model_json", "model_sha256", "required_first_red"});
        const auto id = flow400_text(row.get("id")), source = flow400_text(row.get("source")),
                   model = flow400_text(row.get("model_json"));
        flow400_require(ids.insert(std::string(id)).second &&
                            flow400_text(row.get("provenance")) ==
                                "synthetic_native_regression_not_original_corpus" &&
                            flow400_number(row.get("source_bytes")) == source.size() &&
                            digest(source) == flow400_text(row.get("source_sha256")) &&
                            flow400_text(row.get("model_sha256")).size() == 64u &&
                            digest(model) == flow400_text(row.get("model_sha256")),
                        "Flow typed synthetic source/Model lock");
        flow400_require(row.get("required_first_red").is_null() ||
                            row.get("required_first_red").is_obj(),
                        "Flow typed first-red profile");
        if (!row.get("required_first_red").is_null()) ++first_red;
    });
    flow400_require(first_red == 1u && ids.contains("four_line_decision"),
                    "Flow typed exact first-red identity");
    const auto proof = root.get("original000_proof");
    flow400_keys(proof, {"provenance", "source", "source_bytes", "source_sha256", "source_identity",
                         "model_json", "model_sha256", "qualification"});
    const auto source = flow400_text(proof.get("source")),
               model = flow400_text(proof.get("model_json"));
    flow400_require(flow400_text(proof.get("provenance")) ==
                            "source_authored_complete_model_separate_nonmatrix" &&
                        source.size() == 149u &&
                        flow400_number(proof.get("source_bytes")) == source.size() &&
                        digest(source) == flow400_text(proof.get("source_sha256")) &&
                        digest(model) == flow400_text(proof.get("model_sha256")),
                    "Flow typed original000 separate source/Model lock");
    return std::move(*document);
}

enum class FlowTypedMutation : std::size_t {
    LegacyFlow400,
    WrongShapePair,
    MixedContour,
    MissingDecisionDiagonal,
    WrongDecisionTerminal,
    LabelOutsideContour,
    BlankBBoxPort,
    ShoulderPort,
    ExtraPerimeterArm,
    RouteCrossing,
    OmittedNode,
    WrongEndpoint,
    WrongEdgeStyle,
    WrongRank,
    SameLabelShapeExchange,
    SemanticCaptionSwap,
    PhysicalYesNotSwap,
    MissingCaption,
    CaptionOwnerPathWithOrphan,
    CaptionOffOwnerPath,
    Count,
};

constexpr std::array<std::string_view, static_cast<std::size_t>(FlowTypedMutation::Count)>
    kFlowTypedMutationNames{{
        "legacy_flow400_mutations",
        "wrong_shape_pair",
        "mixed_contour",
        "missing_decision_diagonal",
        "wrong_decision_terminal",
        "label_outside_contour",
        "blank_bbox_port",
        "decision_shoulder_port",
        "extra_perimeter_arm",
        "route_crossing",
        "omitted_node",
        "wrong_endpoint",
        "wrong_edge_style",
        "wrong_rank",
        "same_label_shape_exchange",
        "semantic_caption_swap",
        "physical_yes_not_swap",
        "missing_caption",
        "caption_owner_path_with_orphan",
        "caption_off_owner_path",
    }};

struct FlowTypedMutationCount {
    std::size_t applicable = 0u;
    std::size_t attempted = 0u;
    std::size_t non_resource_rejected = 0u;
};

struct FlowTypedMutationAudit {
    std::array<FlowTypedMutationCount, static_cast<std::size_t>(FlowTypedMutation::Count)> counts{};

    FlowTypedMutationCount& at(FlowTypedMutation mutation) {
        return counts[static_cast<std::size_t>(mutation)];
    }

    const FlowTypedMutationCount& at(FlowTypedMutation mutation) const {
        return counts[static_cast<std::size_t>(mutation)];
    }
};

// Generic flow_typed_* primitives are owned by mermaid_flow400_authority.hpp.

TEST(MermaidFlowTypedShapes, CompleteSyntheticSourcesAndSeparateOriginal000Proof) {
    auto asset = flow_typed_shapes_documents();
    json::JsonMutDoc report;
    auto root = report.object(), rows = root.make_arr();
    root.set("kind", "flow_native_typed_shapes/v2");
    root.set("asset_sha256", kFlowTypedShapesAssetSha);
    root.set("qualification",
             "Synthetic controls only. Original000 remains a separate source-authored Model proof; "
             "no original, terminal, official, or foreign credit is asserted.");
    std::size_t visited = 0u, future_complete = 0u, historical_first_red_markers = 0u,
                resource_errors = 0u;
    FlowTypedMutationAudit total_mutations;
    asset.root().get("synthetic_controls").iter([&](JsonVal item) {
        OracleWork work{kFlow400SceneLimit};
        // All new per-scene frame, contour, caption, mutation and report storage is paid before
        // capture.
        work.spend(262144u + 32u * sizeof(Grid) + 32u * sizeof(Model) + 128u * sizeof(ShapeFrame) +
                   128u * sizeof(Label) + 128u * sizeof(LabelRegion) +
                   2u * sizeof(FlowTypedMutationAudit) + 768u * sizeof(json::JsonMutVal));
        const auto id = flow400_text(item.get("id")), source = flow400_text(item.get("source"));
        auto row = root.make_obj();
        row.set("id", id);
        row.set("source_sha256", flow400_text(item.get("source_sha256")));
        row.set("model_sha256", flow400_text(item.get("model_sha256")));
        row.set("provenance", flow400_text(item.get("provenance")));
        std::array<std::optional<Grid>, 5> frames;
        std::array<char, 257> error{};
        FlowTypedMutationAudit scene_mutations;
        bool complete = false, decoded = false, admitted = false,
             historical_first_red = !item.get("required_first_red").is_null(), resource = false;
        if (historical_first_red) {
            flow400_require(id == "four_line_decision",
                            "Flow typed historical first-red marker moved");
            ++historical_first_red_markers;
        }
        bool source_wide = false, source_narrow = false, restored = false;
        std::size_t published = 0u;
        const char* phase = "capture";
        std::optional<Point> selected_route_crossing;
        try {
            work.product(source.size(), 32u);
            auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
            element->ComputeRequirement();
            ordinary->ComputeRequirement();
            const int height =
                std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
            flow400_require(height <= 1040, "Flow typed complete source height");
            constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            for (std::size_t n = 0; n < frames.size(); ++n) {
                frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
                flow400_publication(*frames[n], work);
                ++published;
            }
            flow400_require(published == 5u, "Flow typed complete frame publication");
            // Source binding and Model decoding deliberately happen after five public captures.
            phase = "bind";
            work.product(source.size(), 4u);
            flow400_require(digest(source) == flow400_text(item.get("source_sha256")),
                            "Flow typed source SHA");
            for (const auto& frame : frames)
                static_cast<void>(flow400_following(*frame, work));
            source_wide = flow400_same(*frames[0], *frames[3], work);
            source_narrow = flow400_same(*frames[1], *frames[4], work);
            restored = flow400_same(*frames[0], *frames[2], work);
            flow400_require(restored, "Flow typed restored wide frame differs");
            phase = "model";
            const auto model_bytes = flow400_text(item.get("model_json"));
            auto document = json::parse(model_bytes);
            flow400_require(document.has_value() &&
                                digest(model_bytes) == flow400_text(item.get("model_sha256")),
                            "Flow typed Model JSON/SHA");
            const auto model = flow_typed_shape_model(document->root(), work);
            decoded = true;
            phase = "wide_geometry";
            const auto wide_errors = flow400_geometry(*frames[0], model, work);
            {
                flow400_require(!source_wide, "Flow typed mandatory source returned ordinary text");
                flow400_require(wide_errors.empty(),
                                wide_errors.empty() ? "" : wide_errors.front().c_str());
                if (!source_narrow) {
                    phase = "narrow_geometry";
                    const auto narrow = flow400_geometry(*frames[1], model, work);
                    flow400_require(narrow.empty(), "Flow typed narrow geometry failed");
                }
                phase = "restored_geometry";
                const auto restored_errors = flow400_geometry(*frames[2], model, work);
                flow400_require(restored_errors.empty(), "Flow typed restored geometry failed");
                phase = "typed_mutations";
                auto apply = [&](FlowTypedMutation mutation, bool applicable, auto&& check) {
                    if (!applicable) return;
                    auto& local = scene_mutations.at(mutation);
                    auto& total = total_mutations.at(mutation);
                    ++local.applicable;
                    ++local.attempted;
                    ++total.applicable;
                    ++total.attempted;
                    check();
                    ++local.non_resource_rejected;
                    ++total.non_resource_rejected;
                };
                apply(FlowTypedMutation::LegacyFlow400, true, [&] {
                    const auto rejected = flow400_mutations(*frames[0], model, work);
                    flow400_require(rejected > 0u, "Flow typed legacy mutation witness");
                });

                work.spend(128u * sizeof(ShapeFrame) + 4096u);
                const auto contours = closed_frames(*frames[0], work);
                const auto shape_frame = [&](Shape shape) -> std::optional<ShapeFrame> {
                    const auto node = std::ranges::find_if(
                        model.nodes, [&](const Node& value) { return value.shape == shape; });
                    if (node == model.nodes.end()) return std::nullopt;
                    return flow_typed_frame_for_node(*frames[0], contours, *node, work);
                };
                const auto rectangle = shape_frame(Shape::Rectangle),
                           rounded = shape_frame(Shape::Rounded),
                           decision = shape_frame(Shape::Decision);

                if (id == "three_shapes") {
                    for (std::size_t node = 0; node < model.nodes.size(); ++node)
                        for (const auto wrong :
                             {Shape::Rectangle, Shape::Rounded, Shape::Decision}) {
                            if (wrong == model.nodes[node].shape) continue;
                            apply(FlowTypedMutation::WrongShapePair, true, [&] {
                                flow_typed_expect_model_rejected(
                                    *frames[0], model, work,
                                    [&](Model& bad) { bad.nodes[node].shape = wrong; },
                                    "directed wrong shape");
                            });
                        }
                    if (rectangle)
                        apply(FlowTypedMutation::MixedContour, true, [&] {
                            flow_typed_expect_rejected(
                                *frames[0], model, work,
                                [&](Grid& bad) { bad.put({rectangle->left, rectangle->top}, "╭"); },
                                "rectangle mixed corner");
                        });
                    if (rounded)
                        apply(FlowTypedMutation::MixedContour, true, [&] {
                            flow_typed_expect_rejected(
                                *frames[0], model, work,
                                [&](Grid& bad) { bad.put({rounded->left, rounded->top}, "┌"); },
                                "rounded mixed corner");
                        });
                    if (decision) {
                        apply(FlowTypedMutation::MixedContour, true, [&] {
                            const Point shoulder{decision->left + 1, decision->top + 1};
                            flow_typed_expect_rejected(
                                *frames[0], model, work, [&](Grid& bad) { bad.put(shoulder, "┌"); },
                                "decision mixed shoulder");
                        });
                        apply(FlowTypedMutation::MissingDecisionDiagonal, true, [&] {
                            const Point shoulder{decision->left + 1, decision->top + 1};
                            flow_typed_expect_rejected(
                                *frames[0], model, work, [&](Grid& bad) { bad.put(shoulder, " "); },
                                "missing decision diagonal");
                        });
                        apply(FlowTypedMutation::WrongDecisionTerminal, true, [&] {
                            const Point terminal{decision->left,
                                                 decision->top + decision->radius()};
                            flow_typed_expect_rejected(
                                *frames[0], model, work, [&](Grid& bad) { bad.put(terminal, "X"); },
                                "wrong decision terminal");
                        });
                        const auto decision_node =
                            std::ranges::find_if(model.nodes, [](const Node& value) {
                                return value.shape == Shape::Decision;
                            });
                        const auto decision_labels =
                            decision_node == model.nodes.end()
                                ? std::vector<Label>{}
                                : flow_typed_caption_blocks(*frames[0], decision_node->label, work);
                        const auto orphan = flow_typed_orphan_label_start(
                            *frames[0],
                            decision_node == model.nodes.end() ? std::string_view{}
                                                               : decision_node->label,
                            contours, work);
                        apply(FlowTypedMutation::LabelOutsideContour,
                              decision_node != model.nodes.end() && decision_labels.size() == 1u &&
                                  orphan.has_value(),
                              [&] {
                                  flow_typed_expect_rejected(
                                      *frames[0], model, work,
                                      [&](Grid& bad) {
                                          flow_typed_erase_label(bad, decision_labels.front(),
                                                                 decision_node->label, work);
                                          flow_typed_write_at(bad, *orphan, decision_node->label,
                                                              work);
                                      },
                                      "decision label outside physical contour");
                              });
                        // The actual Decision sits at the left canvas edge, so its left
                        // bounding-box exterior is unavailable. Use the symmetric right
                        // blank corner/shoulder with an authored left-facing arrow.
                        const Point blank_corner{decision->right, decision->top};
                        const Point blank_head{blank_corner.x + 1, blank_corner.y};
                        const Point blank_shaft{blank_corner.x + 2, blank_corner.y};
                        const bool blank_right =
                            frames[0]->has(blank_head) && frames[0]->has(blank_shaft) &&
                            !decision->boundary(blank_corner) &&
                            !decision->legal_port(blank_corner, R) &&
                            frames[0]->at(blank_head) == " " && frames[0]->at(blank_shaft) == " ";
                        apply(FlowTypedMutation::BlankBBoxPort, blank_right, [&] {
                            flow400_require(
                                blank_right && blank_head.x == blank_corner.x + 1 &&
                                    blank_shaft.x == blank_head.x + 1,
                                "Flow typed right blank-corner arrow path precondition");
                            flow_typed_expect_rejected(
                                *frames[0], model, work,
                                [&](Grid& bad) {
                                    bad.put(blank_shaft, "─");
                                    bad.put(blank_head, "◀");
                                },
                                "right blank decision bounding-box corner target");
                        });
                        const Point shoulder{decision->right - 1, decision->top + 1};
                        const Point shoulder_head{shoulder.x + 1, shoulder.y};
                        const Point shoulder_shaft{shoulder.x + 2, shoulder.y};
                        const bool shoulder_right =
                            frames[0]->has(shoulder_head) && frames[0]->has(shoulder_shaft) &&
                            decision->boundary(shoulder) && !decision->legal_port(shoulder, R) &&
                            frames[0]->at(shoulder_head) == " " &&
                            frames[0]->at(shoulder_shaft) == " ";
                        apply(FlowTypedMutation::ShoulderPort, shoulder_right, [&] {
                            flow400_require(shoulder_right && shoulder_head.x == shoulder.x + 1 &&
                                                shoulder_shaft.x == shoulder_head.x + 1,
                                            "Flow typed right shoulder arrow path precondition");
                            flow_typed_expect_rejected(
                                *frames[0], model, work,
                                [&](Grid& bad) {
                                    bad.put(shoulder_shaft, "─");
                                    bad.put(shoulder_head, "◀");
                                },
                                "right decision diagonal shoulder target");
                        });
                    }
                    if (rectangle)
                        apply(FlowTypedMutation::ExtraPerimeterArm, true, [&] {
                            const Point top_mid{(rectangle->left + rectangle->right) / 2,
                                                rectangle->top};
                            flow_typed_expect_rejected(
                                *frames[0], model, work, [&](Grid& bad) { bad.put(top_mid, "┼"); },
                                "extra rectangle perimeter arm");
                        });
                    if (rectangle)
                        apply(FlowTypedMutation::OmittedNode, true, [&] {
                            flow_typed_expect_rejected(
                                *frames[0], model, work,
                                [&](Grid& bad) { bad.put({rectangle->left, rectangle->top}, " "); },
                                "omitted rectangle node");
                        });
                }

                const bool has_edge = !model.edges.empty();
                apply(FlowTypedMutation::WrongEndpoint,
                      has_edge && model.edges.front().from != model.edges.front().to, [&] {
                          flow_typed_expect_model_rejected(
                              *frames[0], model, work,
                              [&](Model& bad) { bad.edges.front().to = bad.edges.front().from; },
                              "wrong endpoint");
                      });
                apply(FlowTypedMutation::WrongEdgeStyle, has_edge, [&] {
                    flow_typed_expect_model_rejected(
                        *frames[0], model, work,
                        [&](Model& bad) {
                            bad.edges.front().style =
                                bad.edges.front().style == "solid" ? "dotted" : "solid";
                        },
                        "wrong edge style");
                });
                apply(FlowTypedMutation::WrongRank, has_edge, [&] {
                    flow_typed_expect_model_rejected(
                        *frames[0], model, work,
                        [&](Model& bad) {
                            if (bad.direction == "TB")
                                bad.direction = "BT";
                            else if (bad.direction == "BT")
                                bad.direction = "TB";
                            else if (bad.direction == "LR")
                                bad.direction = "RL";
                            else
                                bad.direction = "LR";
                        },
                        "wrong direction rank");
                });

                if (id == "three_shapes_same_label" && model.nodes.size() == 3u)
                    apply(FlowTypedMutation::SameLabelShapeExchange, true, [&] {
                        flow_typed_expect_model_rejected(
                            *frames[0], model, work,
                            [&](Model& bad) { std::swap(bad.nodes[0].shape, bad.nodes[1].shape); },
                            "shape-specific Same label exchange");
                    });
                if ((id == "ports_LR" || id == "ports_RL") && !contours.empty()) {
                    std::optional<Point> crossing;
                    for (int y = 0; !crossing && y < frames[0]->height; ++y)
                        for (int x = 0; !crossing && x < frames[0]->width; ++x) {
                            const Point point{x, y};
                            work.spend();
                            if (glyph(frames[0]->at(point)).bits == (L | R) &&
                                !std::ranges::any_of(contours, [&](const ShapeFrame& frame) {
                                    return frame.contains(point);
                                }))
                                crossing = point;
                        }
                    // The first authored horizontal route cell is the sole witness; a rejected
                    // mutation cannot be retried or hidden.
                    flow400_require(
                        crossing.has_value(),
                        "Flow typed LR/RL control lacks authored horizontal route cell");
                    selected_route_crossing = crossing;
                    apply(FlowTypedMutation::RouteCrossing, true, [&] {
                        flow_typed_expect_rejected(
                            *frames[0], model, work, [&](Grid& bad) { bad.put(*crossing, "┼"); },
                            "selected authored route crossing");
                    });
                }
                if (id == "equal_width_caption_swap") {
                    const auto yes = flow_typed_caption_blocks(*frames[0], "YES", work),
                               not_text = flow_typed_caption_blocks(*frames[0], "NOT", work);
                    const auto yes_edge = std::ranges::find_if(
                        model.edges, [](const Edge& edge) { return edge.label == "YES"; });
                    const auto not_edge = std::ranges::find_if(
                        model.edges, [](const Edge& edge) { return edge.label == "NOT"; });
                    apply(FlowTypedMutation::SemanticCaptionSwap,
                          yes.size() == 1u && not_text.size() == 1u &&
                              yes_edge != model.edges.end() && not_edge != model.edges.end(),
                          [&] {
                              const auto yes_index =
                                  static_cast<std::size_t>(yes_edge - model.edges.begin());
                              const auto not_index =
                                  static_cast<std::size_t>(not_edge - model.edges.begin());
                              flow_typed_expect_model_rejected(
                                  *frames[0], model, work,
                                  [&](Model& bad) {
                                      std::swap(bad.edges[yes_index].label,
                                                bad.edges[not_index].label);
                                  },
                                  "semantic equal-count YES/NOT caption swap");
                          });
                    apply(FlowTypedMutation::PhysicalYesNotSwap,
                          yes.size() == 1u && not_text.size() == 1u, [&] {
                              flow_typed_expect_rejected(
                                  *frames[0], model, work,
                                  [&](Grid& bad) {
                                      flow_typed_write_label(bad, yes.front(), "YES", "NOT", work);
                                      flow_typed_write_label(bad, not_text.front(), "NOT", "YES",
                                                             work);
                                  },
                                  "physical equal-width YES/NOT swap");
                          });
                }
                if (id == "repeated_captions") {
                    const auto yes = flow_typed_caption_blocks(*frames[0], "Yes", work),
                               no = flow_typed_caption_blocks(*frames[0], "No", work);
                    apply(FlowTypedMutation::MissingCaption, no.size() == 2u, [&] {
                        flow_typed_expect_rejected(
                            *frames[0], model, work,
                            [&](Grid& bad) { flow_typed_erase_label(bad, no.front(), "No", work); },
                            "missing repeated caption");
                    });
                    const auto spare =
                        flow_typed_orphan_label_start(*frames[0], "Yes", contours, work);
                    apply(FlowTypedMutation::CaptionOwnerPathWithOrphan,
                          yes.size() == 2u && spare.has_value(), [&] {
                              flow_typed_expect_rejected(
                                  *frames[0], model, work,
                                  [&](Grid& bad) {
                                      flow_typed_erase_label(bad, yes.back(), "Yes", work);
                                      flow_typed_write_at(bad, *spare, "Yes", work);
                                      const auto retained =
                                          flow_typed_caption_blocks(bad, "Yes", work);
                                      flow400_require(retained.size() == 2u,
                                                      "Flow typed actual caption count remains "
                                                      "exact with an orphan");
                                  },
                                  "actual route-caption ownership with equal-count orphan");
                          });
                    const auto off_owner =
                        flow_typed_orphan_label_start(*frames[0], "No", contours, work);
                    apply(FlowTypedMutation::CaptionOffOwnerPath,
                          no.size() == 2u && off_owner.has_value(), [&] {
                              flow_typed_expect_rejected(
                                  *frames[0], model, work,
                                  [&](Grid& bad) {
                                      flow_typed_erase_label(bad, no.back(), "No", work);
                                      flow_typed_write_at(bad, *off_owner, "No", work);
                                  },
                                  "valid caption moved off its only owner path");
                          });
                }
                admitted = true;
                complete = true;
            }
            phase = "complete";
        } catch (const std::exception& failure) {
            std::size_t n = 0u;
            for (; n < 256u && failure.what()[n]; ++n)
                error[n] = failure.what()[n];
            resource = work.remaining == 0u || flow400_resource(std::string_view(error.data(), n));
        }
        auto mutations = row.make_arr();
        std::size_t mutation_applicable = 0u, mutation_attempted = 0u, mutation_rejected = 0u;
        for (std::size_t n = 0; n < kFlowTypedMutationNames.size(); ++n) {
            const auto& count = scene_mutations.counts[n];
            auto mutation = row.make_obj();
            mutation.set("name", kFlowTypedMutationNames[n]);
            mutation.set("applicable", count.applicable > 0u);
            mutation.set("applicable_count", static_cast<int64_t>(count.applicable));
            mutation.set("attempted", static_cast<int64_t>(count.attempted));
            mutation.set("non_resource_rejected",
                         static_cast<int64_t>(count.non_resource_rejected));
            mutations.append(mutation);
            mutation_applicable += count.applicable;
            mutation_attempted += count.attempted;
            mutation_rejected += count.non_resource_rejected;
        }
        row.add("mutations", mutations);
        row.set("mutation_applicable", static_cast<int64_t>(mutation_applicable));
        row.set("mutation_attempted", static_cast<int64_t>(mutation_attempted));
        row.set("mutation_non_resource_rejected", static_cast<int64_t>(mutation_rejected));
        const char* classification = complete && !resource
                                         ? "future_complete"
                                         : (resource ? "resource_failure" : "future_incomplete");
        row.set("phase", phase);
        row.set("classification", classification);
        row.set("error", std::string_view(error.data()));
        row.set("resource_error", resource);
        row.set("published_frames", static_cast<int64_t>(published));
        row.set("typed_Model_decoded", decoded);
        row.set("source_wide", source_wide);
        row.set("source_narrow", source_narrow);
        row.set("restored_identically", restored);
        row.set("historical_first_red_marker", historical_first_red);
        row.set("whole_source_admitted", admitted);
        row.set("complete", complete);
        row.set("remaining", static_cast<int64_t>(work.remaining));
        row.set("selected_route_crossing", selected_route_crossing.has_value());
        if (selected_route_crossing) {
            row.set("selected_route_crossing_x", static_cast<int64_t>(selected_route_crossing->x));
            row.set("selected_route_crossing_y", static_cast<int64_t>(selected_route_crossing->y));
        }
        constexpr std::array<std::string_view, 5> names{
            {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
        for (std::size_t n = 0; n < published; ++n)
            flow400_append_frame(row, names[n], *frames[n]);
        rows.append(row);
        ++visited;
        if (complete && !resource) ++future_complete;
        if (resource) ++resource_errors;
        if (!complete) ADD_FAILURE() << id << ": " << std::string_view(error.data());
    });
    auto catalog = root.make_arr();
    for (std::size_t n = 0; n < kFlowTypedMutationNames.size(); ++n) {
        const auto& count = total_mutations.counts[n];
        auto mutation = root.make_obj();
        mutation.set("name", kFlowTypedMutationNames[n]);
        mutation.set("applicable", static_cast<int64_t>(count.applicable));
        mutation.set("attempted", static_cast<int64_t>(count.attempted));
        mutation.set("non_resource_rejected", static_cast<int64_t>(count.non_resource_rejected));
        catalog.append(mutation);
    }
    root.add("cases", rows);
    root.add("mutation_catalog", catalog);
    root.set("visited", static_cast<int64_t>(visited));
    root.set("synthetic_future_complete", static_cast<int64_t>(future_complete));
    root.set("fixture_historical_first_red_markers",
             static_cast<int64_t>(historical_first_red_markers));
    root.set("expected_first_red_credit", static_cast<int64_t>(0u));
    root.set("completed_first_red_credit", static_cast<int64_t>(0u));
    root.set("resource_errors", static_cast<int64_t>(resource_errors));
    report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto target =
        std::filesystem::path(LOOM_MERMAID_REPORT_DIR) / "mermaid-flow-typed-shapes.json";
    std::ofstream output(target, std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n')) << "Cannot publish " << target;
#endif
    EXPECT_EQ(visited, 9u);
    EXPECT_EQ(future_complete, 9u);
    EXPECT_EQ(historical_first_red_markers, 1u);
    EXPECT_EQ(resource_errors, 0u);
    for (std::size_t n = 0; n < total_mutations.counts.size(); ++n) {
        const auto& count = total_mutations.counts[n];
        EXPECT_EQ(count.applicable, count.attempted) << kFlowTypedMutationNames[n];
        EXPECT_EQ(count.attempted, count.non_resource_rejected) << kFlowTypedMutationNames[n];
        EXPECT_GT(count.non_resource_rejected, 0u) << kFlowTypedMutationNames[n];
    }
    EXPECT_EQ(total_mutations.at(FlowTypedMutation::WrongShapePair).non_resource_rejected, 6u);
    // Original000 proves only frozen source/Model completeness and does not execute or change any
    // denominator.
    const auto proof = asset.root().get("original000_proof");
    OracleWork proof_work{kFlow400SceneLimit};
    auto proof_document = json::parse(flow400_text(proof.get("model_json")));
    ASSERT_TRUE(proof_document.has_value());
    const auto proof_model = flow_typed_shape_model(proof_document->root(), proof_work);
    EXPECT_EQ(proof_model.nodes.size(), 6u);
    EXPECT_EQ(proof_model.edges.size(), 6u);
    EXPECT_EQ(digest(flow400_text(proof.get("source"))), flow400_text(proof.get("source_sha256")));
}

TEST(MermaidFlowTypedShapes, ClosedShapeWireAndPhysicalContours) {
    OracleWork work{kFlow400SceneLimit};
    // Decode the closed JSON wire before any Grid assertion: Decision is a
    // private C++ spelling and must never become a JSON compatibility alias.
    constexpr std::string_view model_json =
        R"JSON({"direction":"TB","nodes":[["D",{"id":"D","label":"D","shape":"diamond"}]],"edges":[],"subgraphs":[]})JSON";
    auto model_document = json::parse(model_json);
    ASSERT_TRUE(model_document.has_value());
    ASSERT_EQ(flow_typed_shape_model(model_document->root(), work).nodes.front().shape,
              Shape::Decision);
    for (const std::string_view rejected : {"decision", "octagon"}) {
        work.product(model_json.size(), 4u);
        auto altered = std::string(model_json);
        const auto at = altered.find("diamond");
        ASSERT_NE(at, std::string::npos);
        altered.replace(at, 7u, rejected);
        auto rejected_document = json::parse(altered);
        ASSERT_TRUE(rejected_document.has_value());
        EXPECT_THROW(static_cast<void>(flow_typed_shape_model(rejected_document->root(), work)),
                     std::runtime_error);
    }
    // This static PixelAt grid is a contract unit, not a renderer fixture.
    auto decision = mock({"  ╱───╲  ", " ╱     ╲ ", "◇       ◇", " ╲     ╱ ", "  ╲───╱  "});
    const ShapeFrame frame{{0, 0, 8, 4}, Shape::Decision};
    ASSERT_TRUE(exact_decision(decision, frame, work));
    EXPECT_TRUE(frame.legal_port({0, 2}, L));
    EXPECT_TRUE(frame.legal_port({8, 2}, R));
    EXPECT_FALSE(frame.legal_port({1, 1}, L));
    EXPECT_FALSE(frame.legal_port({0, 0}, U));
    flow400_copy_grid(decision, work);
    auto missing = decision;
    work.spend(64u);
    missing.put({2, 0}, "X");
    EXPECT_FALSE(exact_decision(missing, frame, work));
    EXPECT_EQ(parse_shape("rectangle"), Shape::Rectangle);
    EXPECT_EQ(parse_shape("rounded"), Shape::Rounded);
    EXPECT_EQ(parse_shape("diamond"), Shape::Decision);
    EXPECT_THROW(static_cast<void>(parse_shape("decision")), std::runtime_error);
    EXPECT_EQ(parse_shape("hexagon"), Shape::Hexagon);
    EXPECT_EQ(parse_shape("triangle"), Shape::Triangle);
}
}  // namespace

namespace {
TEST(MermaidFlowTypedShapes, TargetLegalPortAndSourcePhysicalArmRemainDistinct) {
    OracleWork work{kFlow400SceneLimit};
    work.spend(32768u + 8u * sizeof(Grid) + 8u * sizeof(Model));
    const Model model{"LR", {{"A", "A"}, {"B", "B"}}, {{0, 1, "", "solid", false, true}}, {}};
    // This is a hand-authored PixelAt-style contract Grid. The target is a flat
    // box side, exactly as the renderer leaves it after placing the outside arrow.
    const auto good = mock({"┌───┐     ┌───┐         ", "│ A ├────▶│ B │         ",
                            "└───┘     └───┘         ", "", kSentinel, ""});
    const ShapeFrame target{{10, 0, 14, 2}, Shape::Rectangle};
    ASSERT_EQ(good.at({10, 1}), "│");
    ASSERT_EQ(good.at({4, 1}), "├");
    ASSERT_TRUE(target.legal_port({10, 1}, L));
    ASSERT_FALSE(target.accepts_port(good, {10, 1}, L));
    // The complete matcher must accept the legal target despite its flat border,
    // then recover the source only through its authored outward arm.
    ASSERT_TRUE(flow400_geometry(good, model, work).empty());

    flow400_copy_grid(good, work);
    auto missing_source_arm = good;
    work.spend(64u);
    missing_source_arm.put({4, 1}, "│");
    const auto errors = flow400_geometry(missing_source_arm, model, work);
    ASSERT_FALSE(errors.empty());
    for (const auto& error : errors)
        EXPECT_FALSE(flow400_resource(error));
}
}  // namespace

namespace {
TEST(MermaidTermaidFlowAssertions, FourOriginalRenderFunctionsRequireCompleteScenesAndPredicates) {
    OracleWork setup{kFlow400SceneLimit};
    setup.spend(65536u);
    setup.product(28167u, 16u);
    const auto upstream =
        flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                     "fixtures/mermaid_terminal/termaid/upstream/tests/test_corner_cases.py");
    ASSERT_EQ(upstream.size(), 28167u);
    ASSERT_EQ(digest(upstream), "c107aaf63f759e000611391acc2bd5e418e7954d6c54848e6832ad7ca7d65b2e");

    struct Case {
        std::string_view function;
        int first_line, last_line;
        std::string_view function_sha, source, source_sha, model_json, model_sha;
        std::size_t predicates;
    };

    constexpr std::array<Case, 4> cases{{
        {"test_unicode_labels_render", 50, 53,
         "b14e4f5c811e95a8047e3a9b2f228a9ee03e7456f77675cafa2c146ee99cff5a",
         "graph LR\n  A[\"café\"] --> B[\"naïve\"]",
         "746010e281b1998438c74c9fb455f24b63172241c76363af5f5bd495177c9661",
         "{\"direction\":\"LR\",\"nodes\":[[\"A\",{\"id\":\"A\",\"label\":\"café\",\"shape\":"
         "\"rectangle\"}],[\"B\",{\"id\":\"B\",\"label\":\"naïve\",\"shape\":\"rectangle\"}]],"
         "\"edges\":[{\"source\":\"A\",\"target\":\"B\",\"label\":\"\",\"style\":\"solid\","
         "\"hasArrowStart\":false,\"hasArrowEnd\":true}],\"subgraphs\":[]}",
         "94c1ea864baf636f3ed62a24de8111cf0ab74dd4e1fb689183021a662dc0afa6", 2},
        {"test_very_long_label", 71, 75,
         "502846b280fe55aadeb150a9ca91ccb8c7d2ac85bf26442a4411daf4b74860ce",
         "graph LR\n  "
         "X["
         "\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
         "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
         "AAAAAAAAAAAAAAAAAAAAAAAA\"]",
         "6d9afd40f734c5d024220399f93539eb7f4cb802ba34bbda943761a9323807df",
         "{\"direction\":\"LR\",\"nodes\":[[\"X\",{\"id\":\"X\",\"label\":"
         "\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
         "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
         "AAAAAAAAAAAAAAAAAAAAAAAA\",\"shape\":\"rectangle\"}]],\"edges\":[],\"subgraphs\":[]}",
         "9d220ee29a45bbc2242e8cd94de21771866171969b70e440c30b40121c3e4084", 1},
        {"test_single_node_no_edges", 77, 81,
         "649081a7ad1b5bae53bc68799938051643252e2a2537dcc2a396048377ee6958",
         "graph LR\n  A[Lonely]",
         "bee2df47e787c50113c796fabb5d6bfece777425e5c09f91a080bd35b1abe3e8",
         "{\"direction\":\"LR\",\"nodes\":[[\"A\",{\"id\":\"A\",\"label\":\"Lonely\",\"shape\":"
         "\"rectangle\"}]],\"edges\":[],\"subgraphs\":[]}",
         "bdee4ce06f1944dcdd3effbe33d4293c951c034149ad46a1f7100c9ec11c1b50", 2},
        {"test_many_disconnected_nodes", 83, 87,
         "b044049a9921c9682e48d22280c1159a0e5f46768699c97720a9d109013bd6e3",
         "graph LR\n  A\n  B\n  C\n  D\n  E",
         "b379dfdc49f6c93d9f34f775c217dc47b30df598a062346405ab687304673ef2",
         "{\"direction\":\"LR\",\"nodes\":[[\"A\",{\"id\":\"A\",\"label\":\"A\",\"shape\":"
         "\"rectangle\"}],[\"B\",{\"id\":\"B\",\"label\":\"B\",\"shape\":\"rectangle\"}],[\"C\",{"
         "\"id\":\"C\",\"label\":\"C\",\"shape\":\"rectangle\"}],[\"D\",{\"id\":\"D\",\"label\":"
         "\"D\",\"shape\":\"rectangle\"}],[\"E\",{\"id\":\"E\",\"label\":\"E\",\"shape\":"
         "\"rectangle\"}]],\"edges\":[],\"subgraphs\":[]}",
         "85582c6b4618d19baec1ef8bb69c3a3fecfd6c5593a9a92cd9eae4d0d316de29", 5},
    }};
    json::JsonMutDoc report;
    auto root = report.object(), records = root.make_arr();
    std::size_t complete = 0u, predicates = 0u, mutants = 0u, resources = 0u;
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const auto& item = cases[index];
        OracleWork work{kFlow400SceneLimit};
        work.spend(262144u + 32u * sizeof(Grid) + 32u * sizeof(Model) +
                   128u * sizeof(json::JsonMutVal));
        auto row = root.make_obj();
        auto checks = row.make_arr();
        std::array<std::optional<Grid>, 5> frames;
        std::size_t published = 0u, checked = 0u, rejected = 0u;
        bool good = false, source_wide = false, source_narrow = false;
        const char* phase = "capture";
        std::string error;
        try {
            work.product(item.source.size(), 32u);
            auto element = loom::ui::render_markdown(fenced(item.source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(item.source, "text"));
            element->ComputeRequirement();
            ordinary->ComputeRequirement();
            const int height =
                std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
            flow400_require(height <= 1040, "Termaid render source capture height");
            constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            for (std::size_t n = 0; n < frames.size(); ++n) {
                frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
                flow400_publication(*frames[n], work);
                ++published;
            }
            phase = "source_and_model_binding";
            // Bind the exact original function span, including its computed
            // input construction and complete assertion/loop context.
            work.product(upstream.size(), 4u);
            std::size_t function_begin = std::string::npos, function_end = std::string::npos;
            int original_line = 1;
            for (std::size_t offset = 0; offset < upstream.size(); ++offset) {
                if (original_line == item.first_line && function_begin == std::string::npos)
                    function_begin = offset;
                if (upstream[offset] == '\n') {
                    if (original_line == item.last_line) {
                        function_end = offset + 1u;
                        break;
                    }
                    ++original_line;
                }
            }
            flow400_require(function_begin != std::string::npos &&
                                function_end != std::string::npos && function_end > function_begin,
                            "Termaid exact original function line span");
            const std::string_view function_source(upstream.data() + function_begin,
                                                   function_end - function_begin);
            flow400_require(digest(function_source) == item.function_sha,
                            "Termaid exact original function SHA");
            work.product(item.source.size() + item.model_json.size(), 8u);
            flow400_require(digest(item.source) == item.source_sha,
                            "Termaid complete original source hash");
            flow400_require(digest(item.model_json) == item.model_sha,
                            "Termaid complete source-authored Model hash");
            work.spend(8192u);
            auto document = json::parse(item.model_json);
            flow400_require(document.has_value(), "Termaid source Model JSON");
            const auto model = flow_typed_shape_model(document->root(), work);
            for (const auto& frame : frames)
                static_cast<void>(flow400_following(*frame, work));
            source_wide = flow400_same(*frames[0], *frames[3], work);
            source_narrow = flow400_same(*frames[1], *frames[4], work);
            flow400_require(!source_wide, "Termaid mandatory render source returned ordinary text");
            flow400_require(flow400_same(*frames[0], *frames[2], work),
                            "Termaid restored complete scene differs");
            phase = "complete_geometry";
            for (const std::size_t n : {0u, 2u}) {
                const auto errors = flow400_geometry(*frames[n], model, work);
                if (!errors.empty()) throw std::runtime_error(errors.front());
            }
            if (!source_narrow) {
                const auto errors = flow400_geometry(*frames[1], model, work);
                if (!errors.empty()) throw std::runtime_error(errors.front());
            }
            phase = "original_predicates";
            work.spend(4096u);
            work.product(frames[0]->cells.size(), 8u * sizeof(std::string) + 32u);
            std::string rendered;
            for (int y = 0; y < frames[0]->height; ++y)
                rendered += frames[0]->row(y) + '\n';
            auto predicate = [&](std::string_view value, bool expected) {
                work.product(rendered.size() + value.size(), 4u);
                work.spend(2048u);
                const bool passed = (rendered.find(value) != std::string::npos) == expected;
                auto entry = row.make_obj();
                entry.set("kind", expected ? "contains" : "not_contains");
                entry.set("value", value);
                entry.set("passed", passed);
                checks.append(entry);
                flow400_require(passed, "Termaid original render predicate failed");
                ++checked;
            };
            if (index == 0u) {
                predicate("café", true);
                predicate("naïve", true);
            } else if (index == 1u) {
                work.spend(1024u);
                predicate(std::string(200u, 'A'), true);
            } else if (index == 2u) {
                predicate("Lonely", true);
                predicate("►", false);
            } else
                for (const auto label : {"A", "B", "C", "D", "E"})
                    predicate(label, true);
            flow400_require(checked == item.predicates,
                            "Termaid exact expanded original predicates");
            phase = "good_first_mutations";
            rejected = flow400_mutations(*frames[0], model, work);
            flow400_require(rejected == (model.edges.empty() ? 2u : 3u),
                            "Termaid exact meaningful negatives");
            good = true;
            ++complete;
            predicates += checked;
            mutants += rejected;
            phase = "complete";
        } catch (const std::exception& failure) {
            error = failure.what();
            if (work.remaining == 0u || flow400_resource(error)) ++resources;
            ADD_FAILURE() << item.function << " " << phase << ": " << error;
        }
        row.set("function", item.function);
        row.set("upstream_first_line", item.first_line);
        row.set("upstream_last_line", item.last_line);
        row.set("function_sha256", item.function_sha);
        row.set("source_sha256", item.source_sha);
        row.set("model_sha256", item.model_sha);
        row.set("phase", phase);
        row.set("complete", good);
        row.set("source_wide", source_wide);
        row.set("source_narrow", source_narrow);
        row.set("published_frames", static_cast<std::int64_t>(published));
        row.set("original_predicates_passed", static_cast<std::int64_t>(checked));
        row.set("meaningful_mutants_rejected", static_cast<std::int64_t>(rejected));
        row.set("error", error);
        row.set("work_remaining", static_cast<std::int64_t>(work.remaining));
        row.add("predicates", checks);
        constexpr std::array<std::string_view, 5> frame_names{
            {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
        for (std::size_t n = 0; n < published; ++n)
            flow400_append_frame(row, frame_names[n], *frames[n]);
        records.append(row);
    }
    root.set("qualification",
             "Exactly four pinned Termaid render functions, six original assertion expressions and "
             "ten expanded predicate instances. Full source geometry and source-applicable mutants "
             "are mandatory. No parser/API/options or full foreign-suite certification. Source "
             "fallback gives zero drawing credit.");
    root.set("functions", 4);
    root.set("complete", static_cast<std::int64_t>(complete));
    root.set("original_assertion_expressions", 6);
    root.set("expanded_predicates_passed", static_cast<std::int64_t>(predicates));
    root.set("meaningful_mutants_rejected", static_cast<std::int64_t>(mutants));
    root.set("resource_errors", static_cast<std::int64_t>(resources));
    root.add("cases", records);
    report.set_root(root);
    std::ofstream output(std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                             "mermaid-termaid-flow-render-predicates4.json",
                         std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n'));
    EXPECT_EQ(complete, 4u);
    EXPECT_EQ(predicates, 10u);
    EXPECT_EQ(mutants, 9u);
    EXPECT_EQ(resources, 0u);
}
}  // namespace

namespace {
// Original000 remains outside the official matrix in this test. This native
// scene proves only the preconditions for a separately owned formal admission.
struct FlowOriginal000Diagnostic {
    std::array<char, 1025> preview{};
    std::size_t error_count = 0u, preview_bytes = 0u;
    bool truncated = false;
};

// This intentionally does not charge or inspect OracleWork. The fixed storage
// and report copies are prepaid before capture; it inspects only errors.front()
// and copies at most 1024 bytes immediately after each geometry call.
void flow_original000_capture_diagnostic(FlowOriginal000Diagnostic& destination,
                                         const std::vector<std::string>& errors) {
    destination = {};
    destination.error_count = errors.size();
    if (errors.empty()) return;
    const auto& first = errors.front();
    destination.preview_bytes = std::min(first.size(), destination.preview.size() - 1u);
    std::copy_n(first.begin(), destination.preview_bytes, destination.preview.begin());
    destination.truncated = errors.size() > 1u || first.size() > destination.preview_bytes;
}

void flow_original000_copy_failure(std::array<char, 1025>& destination, std::string_view message) {
    destination.fill('\0');
    const auto count = std::min(destination.size() - 1u, message.size());
    for (std::size_t n = 0; n < count; ++n)
        destination[n] = message[n];
}

TEST(MermaidFlowOriginal000NativeFullScene, SourceBoundTypedSceneHasCompleteGeometryAndMutants) {
    // This ledger remains the existing 128M oracle-work-unit limit, not a
    // memory-byte limit. Every new frame, report, Model copy, contour, fixed
    // diagnostic buffer, and bounded diagnostic JSON share is prepaid before capture.
    OracleWork work{kFlow400SceneLimit};
    work.spend(262144u + 32u * sizeof(Grid) + 32u * sizeof(Model) + 128u * sizeof(ShapeFrame) +
               128u * sizeof(Label) + 128u * sizeof(LabelRegion) +
               1024u * sizeof(json::JsonMutVal) + 8u * sizeof(json::JsonDoc) + 8192u +
               3u * (sizeof(FlowOriginal000Diagnostic) + 1025u * 48u +
                     8u * sizeof(json::JsonMutVal) + 64u));
    json::JsonMutDoc report;
    auto root = report.object(), rows = root.make_arr(), row = root.make_obj();
    root.set("kind", "flow_original000_native_full_scene_candidate/v1");
    root.set("qualification",
             "Native source-bound original000 proof only. Official, terminal, foreign, and Flow400 "
             "matrix credit remain zero until a separate Official-owned formal run.");
    root.set("scene_limit", static_cast<int64_t>(kFlow400SceneLimit));
    root.set("official_credit", static_cast<int64_t>(0));
    root.set("terminal_credit", static_cast<int64_t>(0));
    root.set("foreign_credit", static_cast<int64_t>(0));
    row.set("ordinal", static_cast<int64_t>(0));
    row.set("manifest_global_ordinal", static_cast<int64_t>(661));
    std::array<std::optional<Grid>, 5> frames;
    std::array<char, 1025> failure{};
    std::vector<std::string> wide_errors, narrow_errors, restored_errors;
    FlowOriginal000Diagnostic wide_diagnostic, narrow_diagnostic, restored_diagnostic;
    bool source_bound = false, model_decoded = false, wide_geometry = false,
         narrow_geometry = false, restored_geometry = false;
    bool source_wide = false, source_narrow = false, restored_same = false, resource = false,
         complete = false;
    std::size_t published = 0u, rejected = 0u;
    const char* phase = "input";
    try {
        const auto asset_path = std::filesystem::path(LOOM_TESTS_DIR) /
                                "fixtures/mermaid_native/flow_native_typed_shapes_controls.v1.json";
        work.product(static_cast<std::size_t>(std::filesystem::file_size(asset_path)), 4u);
        const auto asset_bytes = flow400_read(asset_path);
        auto asset = json::parse(asset_bytes);
        flow400_require(asset.has_value(), "Flow original000 typed asset JSON");
        const auto proof = asset->root().get("original000_proof");
        // The immutable source is read from the official fixture for capture. Its
        // proof asset, byte hash, and source identity are bound only afterwards.
        const auto docs = flow400_documents();
        const auto fixtures = docs.manifest.root().get("fixtures");
        const auto base = docs.compact.root().get("cases").at(0u), fixture = fixtures.at(661u);
        const auto fixture_path = flow400_text(fixture.get("path"));
        const auto source =
            flow400_read(std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases" /
                         std::string(fixture_path));
        work.product(source.size(), 32u);
        auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
        element->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int line_floor =
            static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 12;
        const int height = std::max(
            {32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8, line_floor});
        flow400_require(height <= 1040, "Flow original000 complete source height");
        row.set("fixture", fixture_path);
        row.set("height", height);
        phase = "capture";
        constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
        constexpr std::array<std::string_view, 5> frame_names{
            {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
        for (std::size_t n = 0; n < frames.size(); ++n) {
            frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
            flow400_publication(*frames[n], work);
            // This attaches the complete captured ScreenGrid to the in-memory
            // report before source binding. It makes no disk-durability claim.
            flow400_append_frame(row, frame_names[n], *frames[n]);
            ++published;
        }
        flow400_require(published == 5u, "Flow original000 complete frame attachment");
        for (const auto& frame : frames)
            static_cast<void>(flow400_following(*frame, work));
        source_wide = flow400_same(*frames[0], *frames[3], work);
        source_narrow = flow400_same(*frames[1], *frames[4], work);
        restored_same = flow400_same(*frames[0], *frames[2], work);
        phase = "bind";
        flow400_require(std::filesystem::file_size(asset_path) == 12623u &&
                            digest(asset_bytes) == kFlowTypedShapesAssetSha,
                        "Flow original000 typed asset lock");
        flow400_keys(asset->root(), {"schema", "qualification", "source_design_manifest_sha256",
                                     "counts", "synthetic_controls", "original000_proof"});
        flow400_keys(proof, {"provenance", "source", "source_bytes", "source_sha256",
                             "source_identity", "model_json", "model_sha256", "qualification"});
        const auto identity = proof.get("source_identity");
        flow400_bind(base, fixture, 0u, 661u, source, work);
        flow400_bind(identity, fixture, 0u, 661u, source, work);
        flow400_require(base.get("model_json").is_null() && base.get("model_sha256").is_null() &&
                            base.get("adaptation").is_null(),
                        "Flow original000 base pending state changed");
        flow400_require(source == flow400_text(proof.get("source")) &&
                            flow400_number(proof.get("source_bytes")) == source.size() &&
                            digest(source) == flow400_text(proof.get("source_sha256")),
                        "Flow original000 proof source lock");
        source_bound = true;
        row.set("source_sha256", flow400_text(proof.get("source_sha256")));
        row.set("source_git_blob", flow400_text(identity.get("source_git_blob")));
        row.set("oracle_record_sha256", flow400_text(identity.get("oracle_record_sha256")));
        row.set("model_sha256", flow400_text(proof.get("model_sha256")));
        phase = "model";
        const auto model_bytes = flow400_text(proof.get("model_json"));
        auto model_document = json::parse(model_bytes);
        flow400_require(model_document.has_value() &&
                            digest(model_bytes) == flow400_text(proof.get("model_sha256")),
                        "Flow original000 typed Model JSON/SHA");
        const auto model = flow_typed_shape_model(model_document->root(), work);
        model_decoded = true;
        flow400_require(model.nodes.size() == 6u && model.edges.size() == 6u &&
                            model.groups.empty(),
                        "Flow original000 complete Model cardinality");
        const auto diamonds =
            std::count_if(model.nodes.begin(), model.nodes.end(),
                          [](const Node& node) { return node.shape == Shape::Decision; });
        const auto yes = std::count_if(model.edges.begin(), model.edges.end(),
                                       [](const Edge& edge) { return edge.label == "Yes"; });
        const auto no = std::count_if(model.edges.begin(), model.edges.end(),
                                      [](const Edge& edge) { return edge.label == "No"; });
        flow400_require(diamonds == 2 && yes == 2 && no == 2,
                        "Flow original000 typed Model shape/caption multiplicity");
        phase = "wide_geometry";
        wide_errors = flow400_geometry(*frames[0], model, work);
        flow_original000_capture_diagnostic(wide_diagnostic, wide_errors);
        flow400_require(!source_wide, "Flow original000 wide source returned ordinary text");
        flow400_require(wide_errors.empty(),
                        wide_errors.empty() ? "" : wide_errors.front().c_str());
        wide_geometry = true;
        phase = "narrow_geometry";
        if (source_narrow)
            narrow_geometry = true;
        else {
            narrow_errors = flow400_geometry(*frames[1], model, work);
            flow_original000_capture_diagnostic(narrow_diagnostic, narrow_errors);
            flow400_require(narrow_errors.empty(),
                            narrow_errors.empty() ? "" : narrow_errors.front().c_str());
            narrow_geometry = true;
        }
        phase = "restored_geometry";
        restored_errors = flow400_geometry(*frames[2], model, work);
        flow_original000_capture_diagnostic(restored_diagnostic, restored_errors);
        flow400_require(restored_errors.empty(),
                        restored_errors.empty() ? "" : restored_errors.front().c_str());
        flow400_require(restored_same, "Flow original000 restored wide frame differs");
        restored_geometry = true;
        phase = "meaningful_mutations";
        rejected = flow400_mutations(*frames[0], model, work);
        flow400_require(rejected == 3u, "Flow original000 exact legacy mutation count");
        const auto node_index = [&](std::string_view id) {
            const auto it =
                std::ranges::find_if(model.nodes, [&](const Node& node) { return node.id == id; });
            flow400_require(it != model.nodes.end(), "Flow original000 required node absent");
            return static_cast<std::size_t>(it - model.nodes.begin());
        };
        const auto b = node_index("B"), e = node_index("E");
        flow400_require(model.nodes[b].shape == Shape::Decision &&
                            model.nodes[e].shape == Shape::Decision,
                        "Flow original000 diamond Model identity");
        flow_typed_expect_model_rejected(
            *frames[0], model, work, [&](Model& bad) { bad.nodes[b].shape = Shape::Rectangle; },
            "B diamond replaced by rectangle");
        ++rejected;
        flow_typed_expect_model_rejected(
            *frames[0], model, work, [&](Model& bad) { bad.nodes[e].shape = Shape::Rectangle; },
            "E diamond replaced by rectangle");
        ++rejected;
        flow400_require(model.edges[1].label == "Yes" && model.edges[2].label == "No",
                        "Flow original000 selected caption order");
        flow_typed_expect_model_rejected(
            *frames[0], model, work,
            [&](Model& bad) { std::swap(bad.edges[1].label, bad.edges[2].label); },
            "semantic Yes/No caption exchange");
        ++rejected;
        work.spend(128u * sizeof(ShapeFrame) + 4096u);
        const auto contours = closed_frames(*frames[0], work);
        const auto yes_blocks = flow_typed_caption_blocks(*frames[0], "Yes", work),
                   no_blocks = flow_typed_caption_blocks(*frames[0], "No", work);
        flow400_require(yes_blocks.size() == 2u && no_blocks.size() == 2u,
                        "Flow original000 physical caption multiplicity");
        flow_typed_expect_rejected(
            *frames[0], model, work,
            [&](Grid& bad) { flow_typed_erase_label(bad, yes_blocks.front(), "Yes", work); },
            "missing selected repeated Yes caption");
        ++rejected;
        const auto orphan = flow_typed_orphan_label_start(*frames[0], "No", contours, work);
        flow400_require(orphan.has_value(), "Flow original000 No orphan placement witness");
        flow_typed_expect_rejected(
            *frames[0], model, work,
            [&](Grid& bad) {
                flow_typed_erase_label(bad, no_blocks.back(), "No", work);
                flow_typed_write_at(bad, *orphan, "No", work);
                flow400_require(flow_typed_caption_blocks(bad, "No", work).size() == 2u,
                                "Flow original000 moved No preserves caption count");
            },
            "repeated No moved off every selected owner path");
        ++rejected;
        flow400_require(rejected == 8u, "Flow original000 exact meaningful mutation count");
        complete = true;
        phase = "complete";
    } catch (const std::exception& error) {
        flow_original000_copy_failure(failure, error.what() ? std::string_view(error.what())
                                                            : std::string_view{});
        resource = work.remaining == 0u || flow400_resource(std::string_view(failure.data()));
    }
    row.set("phase", phase);
    row.set("failure", std::string_view(failure.data()));
    row.set("resource_error", resource);
    row.set("published_frames", static_cast<int64_t>(published));
    row.set("source_bound", source_bound);
    row.set("typed_Model_decoded", model_decoded);
    row.set("source_wide", source_wide);
    row.set("source_narrow", source_narrow);
    row.set("restored_identically", restored_same);
    row.set("wide_geometry", wide_geometry);
    row.set("narrow_geometry", narrow_geometry);
    row.set("restored_geometry", restored_geometry);
    row.set("meaningful_mutants_expected", static_cast<int64_t>(8));
    row.set("meaningful_mutants_rejected", static_cast<int64_t>(rejected));
    row.set("native_full_scene_complete", complete && !resource);
    row.set("official_credit", static_cast<int64_t>(0));
    row.set("remaining", static_cast<int64_t>(work.remaining));
    // These fixed previews are only report diagnostics. Full geometry predicates
    // above still inspect every returned error and are never shortened for reporting.
    row.set("wide_error_count", static_cast<int64_t>(wide_diagnostic.error_count));
    row.set("wide_error_preview_bytes", static_cast<int64_t>(wide_diagnostic.preview_bytes));
    row.set("wide_error_truncated", wide_diagnostic.truncated);
    row.set("wide_error_preview",
            std::string_view(wide_diagnostic.preview.data(), wide_diagnostic.preview_bytes));
    row.set("narrow_error_count", static_cast<int64_t>(narrow_diagnostic.error_count));
    row.set("narrow_error_preview_bytes", static_cast<int64_t>(narrow_diagnostic.preview_bytes));
    row.set("narrow_error_truncated", narrow_diagnostic.truncated);
    row.set("narrow_error_preview",
            std::string_view(narrow_diagnostic.preview.data(), narrow_diagnostic.preview_bytes));
    row.set("restored_error_count", static_cast<int64_t>(restored_diagnostic.error_count));
    row.set("restored_error_preview_bytes",
            static_cast<int64_t>(restored_diagnostic.preview_bytes));
    row.set("restored_error_truncated", restored_diagnostic.truncated);
    row.set("restored_error_preview", std::string_view(restored_diagnostic.preview.data(),
                                                       restored_diagnostic.preview_bytes));
    rows.append(row);
    root.add("cases", rows);
    root.set("complete", static_cast<int64_t>(complete && !resource));
    root.set("resource_errors", static_cast<int64_t>(resource ? 1u : 0u));
    report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    const auto target = std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                        "mermaid-flow-original000-native-full-scene.json";
    std::ofstream output(target, std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n')) << "Cannot publish " << target;
#endif
    EXPECT_EQ(published, 5u);
    EXPECT_TRUE(source_bound);
    EXPECT_TRUE(model_decoded);
    EXPECT_TRUE(wide_geometry);
    EXPECT_TRUE(narrow_geometry);
    EXPECT_TRUE(restored_geometry);
    EXPECT_EQ(rejected, 8u);
    EXPECT_FALSE(resource);
    EXPECT_TRUE(complete) << std::string_view(failure.data());
}
}  // namespace

namespace {
TEST(MermaidMermanFlowAssertions, BtMultilineEdgeLabelRetainsOriginalRowOrder) {
    constexpr std::string_view source = "flowchart BT\nA -->|first<br>second| B";
    constexpr std::string_view source_sha =
        "4ecd34393db1272b07ab16485c2c57d2ee91805a8999c6d9b25a708114215a32";
    constexpr std::string_view upstream_sha =
        "4290fd499e0a05a5303250b179ba94ffbefbf28a15b5c7f0c636fd3ddd53d952";
    constexpr std::string_view function_sha =
        "9319c931d484c9de60c38cb9d4ac091662a03d8190b4506c958c944b39b87b98";
    constexpr std::string_view raw_model_sha =
        "7cdfb0b544b354a5f3584c6ca57f8d91d5f6f70e78e95c0e7ce4a76e6486c668";
    constexpr std::size_t function_start = 9576u, function_end = 10144u;
    OracleWork work{kFlow400SceneLimit};
    // Reserve report, five frames, complete source/model binding and all bounded projection
    // temporaries before their first use.
    work.spend(327680u + 8u * sizeof(json::JsonDoc) + 32u * sizeof(Grid) + 32u * sizeof(Model) +
               128u * sizeof(json::JsonMutVal) + 32u * sizeof(std::string));
    work.product(20877u + 993u, 16u);
    json::JsonMutDoc report;
    auto root = report.object(), rows = root.make_arr();
    auto row = root.make_obj();
    std::array<std::optional<Grid>, 5> frames;
    std::size_t published = 0u, rejected = 0u, caption_negative_controls = 0u;
    bool complete = false, source_wide = false, source_narrow = false, model_decoded = false,
         resource_error = false, row_recorded = false;
    const char* phase = "capture";
    std::array<char, 257> error{};
    try {
        work.product(source.size(), 32u);
        auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
        auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
        element->ComputeRequirement();
        ordinary->ComputeRequirement();
        const int height =
            std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
        flow400_require(height <= 1040, "Merman BT source capture height");
        constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
        for (std::size_t n = 0; n < frames.size(); ++n) {
            frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
            flow400_publication(*frames[n], work);
            ++published;
        }
        flow400_require(published == frames.size(), "Merman BT five capture/publication frames");
        constexpr std::array<std::string_view, 5> names{
            {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
        for (std::size_t n = 0; n < frames.size(); ++n)
            flow400_append_frame(row, names[n], *frames[n]);
        row.set("prebinding_frames_recorded", true);
        rows.append(row);
        row_recorded = true;
        phase = "source_and_complete_model_binding";
        const auto fixture_root =
            std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_terminal/merman";
        const auto source_path =
            fixture_root / "upstream/tests/flowchart_model/direction_and_labels.rs";
        const auto raw_model_path =
            fixture_root / "runtime-capture/oracle-models/"
                           "7cdfb0b544b354a5f3584c6ca57f8d91d5f6f70e78e95c0e7ce4a76e6486c668.json";
        flow400_require(std::filesystem::file_size(source_path) == 20877u &&
                            std::filesystem::file_size(raw_model_path) == 993u,
                        "Merman BT immutable source/model bytes");
        work.product(20877u + 993u, 4u);
        const auto original = flow400_read(source_path), raw_model = flow400_read(raw_model_path);
        flow400_require(digest(source) == source_sha && digest(original) == upstream_sha &&
                            digest(raw_model) == raw_model_sha,
                        "Merman BT exact source/model identity");
        flow400_require(function_end <= original.size() && function_end > function_start &&
                            digest(std::string_view(original).substr(
                                function_start, function_end - function_start)) == function_sha,
                        "Merman BT exact original assertion function");
        const auto function =
            std::string_view(original).substr(function_start, function_end - function_start);
        flow400_require(function.find("\"flowchart BT\\nA -->|first<br>second| B\"") !=
                            std::string_view::npos,
                        "Merman BT original literal input relation");
        work.spend(16384u);
        auto document = json::parse(raw_model);
        flow400_require(document.has_value(), "Merman BT raw Model JSON");
        const auto raw = document->root();
        flow400_keys(raw,
                     {"accDescr", "accTitle", "classDefs", "direction", "edgeDefaults", "edges",
                      "keyword", "nodes", "subgraphs", "tooltips", "type", "vertexCalls"});
        flow400_require(raw.get("accDescr").is_null() && raw.get("accTitle").is_null() &&
                            raw.get("classDefs").is_obj() && raw.get("classDefs").size() == 0u &&
                            flow400_text(raw.get("direction")) == "BT" &&
                            flow400_text(raw.get("keyword")) == "flowchart" &&
                            flow400_text(raw.get("type")) == "flowchart-v2" &&
                            raw.get("subgraphs").is_arr() && raw.get("subgraphs").size() == 0u &&
                            raw.get("tooltips").is_obj() && raw.get("tooltips").size() == 0u,
                        "Merman BT complete raw Model roots");
        const auto defaults = raw.get("edgeDefaults");
        flow400_keys(defaults, {"interpolate", "style"});
        flow400_require(defaults.get("interpolate").is_null() && defaults.get("style").is_arr() &&
                            defaults.get("style").size() == 0u,
                        "Merman BT raw defaults");
        const auto calls = raw.get("vertexCalls");
        flow400_require(calls.is_arr() && calls.size() == 2u && flow400_text(calls.at(0)) == "A" &&
                            flow400_text(calls.at(1)) == "B",
                        "Merman BT raw vertex order");
        const auto nodes = raw.get("nodes");
        flow400_require(nodes.is_arr() && nodes.size() == 2u, "Merman BT raw node count");
        const auto check_node = [&](JsonVal node, std::string_view id) {
            constexpr std::array<std::string_view, 17> keys{
                {"assetHeight", "assetWidth", "classes", "constraint", "form", "haveCallback",
                 "icon", "id", "img", "label", "labelType", "layoutShape", "link", "linkTarget",
                 "pos", "shape", "styles"}};
            std::array<bool, 17> seen{};
            flow400_require(node.is_obj() && node.size() == keys.size(),
                            "Merman BT complete raw node size");
            node.iter_obj([&](JsonVal key, JsonVal) {
                const auto at = std::ranges::find(keys, flow400_text(key));
                flow400_require(at != keys.end(), "Merman BT raw node unknown key");
                const auto index = static_cast<std::size_t>(at - keys.begin());
                flow400_require(!seen[index], "Merman BT raw node duplicate key");
                seen[index] = true;
            });
            flow400_require(std::ranges::all_of(seen, [](bool value) { return value; }),
                            "Merman BT raw node missing key");
            flow400_require(
                flow400_text(node.get("id")) == id && flow400_text(node.get("label")) == id &&
                    flow400_text(node.get("labelType")) == "text" &&
                    flow400_text(node.get("layoutShape")) == "squareRect" &&
                    node.get("shape").is_null() && node.get("icon").is_null() &&
                    node.get("form").is_null() && node.get("pos").is_null() &&
                    node.get("img").is_null() && node.get("constraint").is_null() &&
                    node.get("assetWidth").is_null() && node.get("assetHeight").is_null() &&
                    node.get("classes").is_arr() && node.get("classes").size() == 0u &&
                    node.get("styles").is_arr() && node.get("styles").size() == 0u &&
                    node.get("link").is_null() && node.get("linkTarget").is_null() &&
                    node.get("haveCallback").is_bool() && !node.get("haveCallback").as_bool(),
                "Merman BT complete raw node values");
        };
        check_node(nodes.at(0), "A");
        check_node(nodes.at(1), "B");
        const auto edges = raw.get("edges");
        flow400_require(edges.is_arr() && edges.size() == 1u, "Merman BT raw edge count");
        const auto edge = edges.at(0);
        flow400_keys(edge, {"animate", "animation", "arrow", "classes", "from", "id", "interpolate",
                            "isUserDefinedId", "label", "labelType", "length", "stroke", "style",
                            "to", "type"});
        flow400_require(
            edge.is_obj() && edge.size() == 15u && flow400_text(edge.get("id")) == "L_A_B_0" &&
                flow400_text(edge.get("from")) == "A" && flow400_text(edge.get("to")) == "B" &&
                flow400_text(edge.get("label")) == "first<br>second" &&
                flow400_text(edge.get("labelType")) == "text" &&
                flow400_text(edge.get("type")) == "arrow_point" &&
                flow400_text(edge.get("arrow")) == "-->" &&
                flow400_number(edge.get("length")) == 1u &&
                flow400_text(edge.get("stroke")) == "normal" && edge.get("animate").is_null() &&
                edge.get("animation").is_null() && edge.get("interpolate").is_null() &&
                edge.get("isUserDefinedId").is_bool() && !edge.get("isUserDefinedId").as_bool() &&
                edge.get("classes").is_arr() && edge.get("classes").size() == 0u &&
                edge.get("style").is_arr() && edge.get("style").size() == 0u,
            "Merman BT complete raw edge");
        work.spend(8u * sizeof(Model) + 8u * sizeof(Node) + 8u * sizeof(Edge) + 4096u);
        work.product(raw_model.size(), 16u);
        std::string label(flow400_text(edge.get("label")));
        const auto br = label.find("<br>");
        flow400_require(br != std::string::npos && label.find("<br>", br + 4u) == std::string::npos,
                        "Merman BT finite label projection");
        label.replace(br, 4u, "\n");
        Model model{std::string(flow400_text(raw.get("direction"))),
                    {{"A", "A"}, {"B", "B"}},
                    {{0, 1, std::move(label), "solid", false, true}},
                    {}};
        model_decoded = true;
        for (const auto& frame : frames)
            static_cast<void>(flow400_following(*frame, work));
        source_wide = flow400_same(*frames[0], *frames[3], work);
        source_narrow = flow400_same(*frames[1], *frames[4], work);
        flow400_require(!source_wide, "Merman BT mandatory source returned ordinary text");
        flow400_require(flow400_same(*frames[0], *frames[2], work),
                        "Merman BT restored complete scene differs");
        phase = "complete_geometry";
        for (const std::size_t n : {0u, 2u}) {
            const auto errors = flow400_geometry(*frames[n], model, work);
            if (!errors.empty()) throw std::runtime_error(errors.front());
        }
        if (!source_narrow) {
            const auto errors = flow400_geometry(*frames[1], model, work);
            if (!errors.empty()) throw std::runtime_error(errors.front());
        }
        phase = "original_row_order_predicate";
        work.product(frames[0]->cells.size(), 64u);
        work.spend(8192u);
        const auto line_index = [&](const Grid& grid, std::string_view text) {
            std::optional<int> result;
            for (int y = 0; y < grid.height; ++y)
                if (grid.row(y).find(text) != std::string::npos) {
                    flow400_require(!result.has_value(), "Merman BT original text duplicated");
                    result = y;
                }
            flow400_require(result.has_value(), "Merman BT original text missing");
            return *result;
        };
        const auto authored_order = [&](const Grid& grid) {
            const auto first = line_index(grid, "first"), second = line_index(grid, "second");
            return second == first + 1;
        };
        flow400_require(authored_order(*frames[0]), "Merman BT original label row order");
        phase = "good_first_caption_row_swap";
        flow400_copy_grid(*frames[0], work);
        work.product(static_cast<std::size_t>(frames[0]->width), 4u * sizeof(std::string) + 16u);
        auto swapped = *frames[0];
        const auto first = line_index(swapped, "first"), second = line_index(swapped, "second");
        for (int x = 0; x < swapped.width; ++x) {
            std::swap(swapped.cells[swapped.index({x, first})],
                      swapped.cells[swapped.index({x, second})]);
        }
        flow400_require(!authored_order(swapped),
                        "Merman BT caption-row swap retained original row-order predicate");
        ++caption_negative_controls;
        phase = "good_first_reversed_model_label";
        flow400_copy_model(model, work);
        work.spend(1024u);
        auto reversed = model;
        reversed.edges.front().label = "second\nfirst";
        const auto reversed_errors = flow400_geometry(*frames[0], reversed, work);
        flow400_require(!reversed_errors.empty(), "Merman BT reversed Model caption accepted");
        for (const auto& error : reversed_errors) {
            flow400_require(!flow400_resource(error),
                            "Merman BT resource is not reversed-caption negative credit");
        }
        ++caption_negative_controls;
        phase = "good_first_mutations";
        rejected = flow400_mutations(*frames[0], model, work);
        flow400_require(rejected == 3u, "Merman BT exact non-resource mutants");
        complete = true;
        phase = "complete";
    } catch (const std::exception& failure) {
        const char* raw = failure.what();
        resource_error = work.remaining == 0u || (raw && flow400_resource(raw));
        std::size_t n = 0u;
        for (; raw && n < 256u && raw[n]; ++n)
            error[n] = raw[n];
    }
    resource_error = resource_error || work.remaining == 0u;
    if (!row_recorded) {
        constexpr std::array<std::string_view, 5> names{
            {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
        for (std::size_t n = 0; n < published; ++n)
            flow400_append_frame(row, names[n], *frames[n]);
        row.set("prebinding_frames_recorded", false);
        rows.append(row);
        row_recorded = true;
    }
    row.set("source_sha256", source_sha);
    row.set("upstream_sha256", upstream_sha);
    row.set("function_sha256", function_sha);
    row.set("raw_model_sha256", raw_model_sha);
    row.set("published_frames", static_cast<int64_t>(published));
    row.set("model_decoded", model_decoded);
    row.set("source_wide", source_wide);
    row.set("source_narrow", source_narrow);
    row.set("original_predicate_instances", 1);
    row.set("original_predicates_passed", complete ? 1 : 0);
    row.set("caption_negative_controls_rejected", static_cast<int64_t>(caption_negative_controls));
    row.set("legacy_non_resource_mutants_rejected", static_cast<int64_t>(rejected));
    row.set("non_resource_rejections", static_cast<int64_t>(caption_negative_controls + rejected));
    row.set("resource_error", resource_error);
    row.set("phase", phase);
    row.set("error", std::string_view(error.data()));
    row.set("complete", complete);
    row.set("work_remaining", static_cast<int64_t>(work.remaining));
    root.set("kind", "merman_bt_multiline_one_original_assertion_native");
    root.set("qualification",
             "One complete original assertion call-site and one runtime instance. Two good-first "
             "negative controls and three legacy non-resource mutants are controls, not original "
             "instances. Merman ASCII API/options/golden grid and all other source functions "
             "remain unported.");
    root.set("original_assertion_callsites", 1);
    root.set("runtime_expanded_instances", 1);
    root.set("caption_negative_controls", 2);
    root.set("legacy_non_resource_mutants_required", 3);
    root.set("complete", complete ? 1 : 0);
    root.set("resource_errors", resource_error ? 1 : 0);
    root.add("cases", rows);
    report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    std::filesystem::create_directories(LOOM_MERMAID_REPORT_DIR);
    std::ofstream output(std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                             "mermaid-m4-bt-multiline-native.json",
                         std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n'));
#endif
    EXPECT_EQ(published, 5u);
    EXPECT_TRUE(complete) << std::string_view(error.data());
    EXPECT_EQ(caption_negative_controls, 2u);
    EXPECT_EQ(rejected, 3u);
    EXPECT_FALSE(resource_error);
    EXPECT_NE(work.remaining, 0u);
}
}  // namespace

namespace {
TEST(MermaidFlowRoundedMultiline, FullSourcesKeepContoursRowsEndpointRolesAndRestoration) {
    struct Control {
        std::string_view direction, label, source, sha;
        bool rounded_source;
    };

    constexpr std::array<Control, 12> controls{{
        {"LR", "one\ntwo\nthree\nfour",
         "flowchart LR\nR(\"one<br>two<br>three<br>four\") -->|go| B[Done]",
         "8d9b19088ec7eb1dc5ca41720584eb5e00d74500f95eddc41970b4e8db738b5e", true},
        {"RL", "one\ntwo\nthree\nfour",
         "flowchart RL\nR(\"one<br>two<br>three<br>four\") -->|go| B[Done]",
         "c7e8abc2b16c89ec2ccd8ff618206645ae9492ecbabed7a0ba1d08f6c7d1a6e4", true},
        {"TB", "one\ntwo\nthree\nfour",
         "flowchart TB\nR(\"one<br>two<br>three<br>four\") -->|go| B[Done]",
         "238f48c701cd167a41fd46e514302112bde9318bde09b07ee58b5564e6510d2c", true},
        {"BT", "one\ntwo\nthree\nfour",
         "flowchart BT\nR(\"one<br>two<br>three<br>four\") -->|go| B[Done]",
         "ad966c98da7f1434e52f15d13ab51387b8aad3a4f1ba64ab53293549e1103476", true},
        {"LR", "one\n\nthree\nfour\nfive",
         "flowchart LR\nB[Done] -->|go| R(\"one<br><br>three<br>four<br>five\")",
         "659d7cd450f206a819c9964cadd4b8bc767a5fb7c580bfc91e054d2f49a33e13", false},
        {"RL", "one\n\nthree\nfour\nfive",
         "flowchart RL\nB[Done] -->|go| R(\"one<br><br>three<br>four<br>five\")",
         "ba250d1e8367e9819d2816a96907f08cb8d27bac7448b36ae2a77c95e119e536", false},
        {"TB", "one\n\nthree\nfour\nfive",
         "flowchart TB\nB[Done] -->|go| R(\"one<br><br>three<br>four<br>five\")",
         "878d30df4db761f315484590b4e5466f6bbacc8488bc4fa67e47c91d9adab572", false},
        {"BT", "one\n\nthree\nfour\nfive",
         "flowchart BT\nB[Done] -->|go| R(\"one<br><br>three<br>four<br>five\")",
         "fa950e569ce72bbe04f4fefdb0fe3ccf548d93f9cc534b035edf821d31629e75", false},
        {"LR", "中文\ncafé", "flowchart LR\nR(\"中文<br>café\") -->|go| B[Done]",
         "b26ef0ea871c48a7d5eccbe08b0e7c9f376ce30d4f670b398ab8c72dbb2fda80", true},
        {"RL", "中文\ncafé", "flowchart RL\nR(\"中文<br>café\") -->|go| B[Done]",
         "d8770722acc1d54ad2bef2c3cdc0c39e849c8cb23adc16cea6b697751d9771c0", true},
        {"TB", "中文\ncafé", "flowchart TB\nR(\"中文<br>café\") -->|go| B[Done]",
         "fe284caac4d2b20b9388bdda9af83f8584fae56abd901ebcd13c3db21a6f135a", true},
        {"BT", "中文\ncafé", "flowchart BT\nR(\"中文<br>café\") -->|go| B[Done]",
         "d9a600c7943a2654b28093bb77714d08244cfb9a0d48594c76e301841a34ae54", true},
    }};
    json::JsonMutDoc report;
    auto root = report.object(), rows = root.make_arr();
    std::size_t complete = 0u, resources = 0u, mutants = 0u;
    for (const auto& control : controls) {
        OracleWork work{kFlow400SceneLimit};
        work.spend(262144u + 32u * sizeof(Grid) + 32u * sizeof(Model) + 128u * sizeof(ShapeFrame) +
                   128u * sizeof(Label) + 128u * sizeof(LabelRegion) +
                   1024u * sizeof(json::JsonMutVal));
        auto row = root.make_obj();
        std::array<std::optional<Grid>, 5> frames;
        std::array<char, 257> error{};
        bool success = false, resource = false, ordinary_narrow = false;
        std::size_t rejected = 0u, published = 0u;
        const char* phase = "capture";
        try {
            work.product(control.source.size(), 32u);
            auto element = loom::ui::render_markdown(fenced(control.source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(control.source, "text"));
            element->ComputeRequirement();
            ordinary->ComputeRequirement();
            const int height =
                std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
            flow400_require(height <= 1040, "Rounded multiline complete source height");
            constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            constexpr std::array<std::string_view, 5> names{
                {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
            for (std::size_t n = 0u; n < 5u; ++n) {
                frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
                flow400_publication(*frames[n], work);
                flow400_append_frame(row, names[n], *frames[n]);
                ++published;
            }
            phase = "bind_and_model";
            flow400_require(digest(control.source) == control.sha,
                            "Rounded multiline exact authored source SHA");
            work.product(control.source.size() + control.label.size(), 16u);
            work.spend(8u * sizeof(Model) + 16u * sizeof(Node) + 8u * sizeof(Edge) + 4096u);
            const Model model{std::string(control.direction),
                              {{"R", std::string(control.label), Shape::Rounded},
                               {"B", "Done", Shape::Rectangle}},
                              {{control.rounded_source ? 0 : 1, control.rounded_source ? 1 : 0,
                                "go", "solid", false, true}},
                              {}};
            for (const auto& frame : frames)
                static_cast<void>(flow400_following(*frame, work));
            flow400_require(!flow400_same(*frames[0], *frames[3], work),
                            "Rounded multiline required drawing fell back at wide width");
            ordinary_narrow = flow400_same(*frames[1], *frames[4], work);
            flow400_require(flow400_same(*frames[0], *frames[2], work),
                            "Rounded multiline exact restored cells");
            phase = "complete_geometry";
            for (const auto n : {0u, 2u}) {
                const auto errors = flow400_geometry(*frames[n], model, work);
                flow400_require(errors.empty(), errors.empty() ? "" : errors.front().c_str());
            }
            if (!ordinary_narrow) {
                const auto errors = flow400_geometry(*frames[1], model, work);
                flow400_require(errors.empty(), errors.empty() ? "" : errors.front().c_str());
            }
            phase = "good_first_mutations";
            rejected = flow400_mutations(*frames[0], model, work);
            flow400_require(rejected == 3u, "Rounded multiline legacy exact mutation count");
            flow_typed_expect_model_rejected(
                *frames[0], model, work, [](Model& bad) { bad.nodes[0].shape = Shape::Rectangle; },
                "Rounded multiline Model corner identity");
            ++rejected;
            flow_typed_expect_model_rejected(
                *frames[0], model, work, [](Model& bad) { std::erase(bad.nodes[0].label, '\n'); },
                "Rounded multiline flattened Model rows");
            ++rejected;
            work.spend(128u * sizeof(ShapeFrame) + 4096u);
            const auto contours = closed_frames(*frames[0], work);
            const auto rounded =
                flow_typed_frame_for_node(*frames[0], contours, model.nodes[0], work);
            flow400_require(rounded.has_value(), "Rounded multiline unique actual rounded contour");
            flow_typed_expect_rejected(
                *frames[0], model, work,
                [&](Grid& bad) { bad.put({rounded->left, rounded->top}, "┌"); },
                "Rounded multiline mixed physical corner");
            ++rejected;
            const bool blank = control.label.find("\n\n") != std::string_view::npos;
            if (blank) {
                flow_typed_expect_model_rejected(
                    *frames[0], model, work,
                    [](Model& bad) {
                        const auto at = bad.nodes[0].label.find("\n\n");
                        flow400_require(at != std::string::npos,
                                        "Rounded multiline blank-row mutation witness");
                        bad.nodes[0].label.erase(at, 1u);
                    },
                    "Rounded multiline missing authored blank row");
                ++rejected;
            }
            flow400_require(rejected == (blank ? 7u : 6u),
                            "Rounded multiline exact applicable negative count");
            success = true;
            phase = "complete";
        } catch (const std::exception& failure) {
            const char* message = failure.what();
            resource = work.remaining == 0u || (message && flow400_resource(message));
            for (std::size_t n = 0u; message && message[n] && n < 256u; ++n)
                error[n] = message[n];
        }
        complete += success;
        resources += resource;
        mutants += rejected;
        row.set("source_sha256", control.sha);
        row.set("direction", control.direction);
        row.set("rounded_source", control.rounded_source);
        row.set("published_frames", static_cast<int64_t>(published));
        row.set("ordinary_narrow", ordinary_narrow);
        row.set("mutants_rejected", static_cast<int64_t>(rejected));
        row.set("complete", success);
        row.set("resource_error", resource);
        row.set("phase", phase);
        row.set("error", std::string_view(error.data()));
        row.set("remaining", static_cast<int64_t>(work.remaining));
        rows.append(row);
        EXPECT_TRUE(success) << control.sha << " " << phase << " "
                             << std::string_view(error.data());
        EXPECT_FALSE(resource);
        EXPECT_EQ(published, 5u);
    }
    root.set("kind", "rounded_multiline_full_source_native/v1");
    root.set("qualification",
             "Twelve synthetic full sources, three authored multiline labels, four directions and "
             "both source/target rounded roles. No official or original foreign assertion credit.");
    root.set("sources", 12);
    root.set("complete", static_cast<int64_t>(complete));
    root.set("meaningful_mutants_rejected", static_cast<int64_t>(mutants));
    root.set("resource_errors", static_cast<int64_t>(resources));
    root.set("formal_admissions", 0);
    root.add("cases", rows);
    report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    std::ofstream output(std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                             "mermaid-flow-rounded-multiline-native.json",
                         std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n'));
#endif
    EXPECT_EQ(complete, 12u);
    EXPECT_EQ(mutants, 76u);
    EXPECT_EQ(resources, 0u);
}
}  // namespace

namespace {
TEST(MermaidFlowLabelRows, ExactAuthoredRowsRejectMissingBlankAndInsertedGaps) {
    const auto check = [](std::initializer_list<std::string_view> rows, std::string_view label,
                          std::size_t count) {
        const auto grid = mock(rows);
        OracleWork work;
        const Rect whole{-1, -1, grid.width, grid.height};
        const auto matches = label_blocks(grid, label, whole, work);
        EXPECT_EQ(matches.size(), count);
        EXPECT_GT(work.remaining, 0u);
    };
    check({"one", "", "three", "four", "five"}, "one\n\nthree\nfour\nfive", 1u);
    check({"one", "", "three", "four", "five"}, "one\nthree\nfour\nfive", 0u);
    check({"one", "two"}, "one\ntwo", 1u);
    check({"one", "", "two"}, "one\ntwo", 0u);
    check({"one", "two"}, "one\n\ntwo", 0u);
    check({"one", "two", "one", "two"}, "one\ntwo", 2u);
    check({"one", "", "two", "one", "two"}, "one\ntwo", 1u);
    check({"中文", "café"}, "中文\ncafé", 1u);
    check({"中文", "", "café"}, "中文\ncafé", 0u);
}
}  // namespace

namespace {
constexpr std::string_view kFlowFoldedShapeDataAssetSha =
    "51b2939bbd28684efb6f15bfd2a8c1b82c61ad9a31b45fd53fd383f6d19f95f4";

json::JsonDoc flow_folded_shape_data_documents() {
    OracleWork setup;
    const auto path = std::filesystem::path(LOOM_TESTS_DIR) /
                      "fixtures/mermaid_native/flow_folded_shape_data_native_controls.v1.json";
    flow400_require(std::filesystem::file_size(path) == 6834u,
                    "Flow folded shape-data asset bytes");
    setup.spend(32768u + 8u * sizeof(json::JsonDoc) + 32u * sizeof(JsonVal));
    setup.product(6834u, 16u);
    const auto bytes = flow400_read(path);
    flow400_require(digest(bytes) == kFlowFoldedShapeDataAssetSha,
                    "Flow folded shape-data asset SHA");
    auto document = json::parse(bytes);
    flow400_require(document.has_value(), "Flow folded shape-data asset JSON");
    const auto root = document->root();
    flow400_keys(root, {"schema", "qualification", "source_design_r1_manifest_sha256",
                        "primary_frontend_commit", "counts", "controls"});
    flow400_require(flow400_text(root.get("schema")) ==
                            "flow_folded_shape_data_native_controls/v1" &&
                        flow400_text(root.get("source_design_r1_manifest_sha256")) ==
                            "25c4d054e7f3514181340878a7eb5169e7e8388c881393c71f2a7672a82fc137" &&
                        flow400_text(root.get("primary_frontend_commit")) ==
                            "1fb7869866dd619168d840291c9c54b4f129758b",
                    "Flow folded shape-data asset schema/source lock");
    const auto counts = root.get("counts");
    flow400_keys(counts, {"controls", "synthetic_controls", "official_full_source_candidates",
                          "original_corpus_credit", "official_credit", "terminal_credit"});
    flow400_require(flow400_number(counts.get("controls")) == 6u &&
                        flow400_number(counts.get("synthetic_controls")) == 4u &&
                        flow400_number(counts.get("official_full_source_candidates")) == 2u &&
                        flow400_number(counts.get("original_corpus_credit")) == 0u &&
                        flow400_number(counts.get("official_credit")) == 0u &&
                        flow400_number(counts.get("terminal_credit")) == 0u,
                    "Flow folded shape-data asset counts");
    const auto controls = root.get("controls");
    flow400_require(controls.is_arr() && controls.size() == 6u,
                    "Flow folded shape-data control count");
    std::set<std::string> ids;
    std::size_t official = 0u, synthetic = 0u;
    controls.iter([&](JsonVal row) {
        setup.spend(4096u);
        flow400_keys(row, {"id", "provenance", "source", "source_bytes", "source_sha256",
                           "model_json", "model_sha256", "official"});
        const auto id = flow400_text(row.get("id")), source = flow400_text(row.get("source")),
                   model = flow400_text(row.get("model_json"));
        flow400_require(ids.insert(std::string(id)).second &&
                            flow400_number(row.get("source_bytes")) == source.size() &&
                            digest(source) == flow400_text(row.get("source_sha256")) &&
                            digest(model) == flow400_text(row.get("model_sha256")),
                        "Flow folded shape-data source/Model lock");
        const auto provenance = flow400_text(row.get("provenance"));
        const auto identity = row.get("official");
        if (identity.is_null()) {
            ++synthetic;
            flow400_require(provenance.starts_with("preexisting_native_") ||
                                provenance.starts_with("source_derived_"),
                            "Flow folded shape-data synthetic provenance");
        } else {
            ++official;
            flow400_keys(identity,
                         {"ordinal", "manifest_global_ordinal", "fixture", "source_git_blob"});
            flow400_require(provenance == "official_full_source_complete_typed_model_candidate" &&
                                (flow400_number(identity.get("ordinal")) == 374u ||
                                 flow400_number(identity.get("ordinal")) == 375u) &&
                                flow400_text(identity.get("fixture")).ends_with(".mmd") &&
                                flow400_text(identity.get("source_git_blob")).size() == 40u,
                            "Flow folded shape-data official provenance");
        }
    });
    flow400_require(official == 2u && synthetic == 4u &&
                        ids.contains("folded_description_legacy_control") &&
                        ids.contains("shape_data_last_wins") &&
                        ids.contains("shape_data_duplicate_same_value") &&
                        ids.contains("shape_data_endpoint") && ids.contains("original_374") &&
                        ids.contains("original_375"),
                    "Flow folded shape-data exact controls");
    return std::move(*document);
}

void flow_folded_shape_data_physical_negative(const Grid& grid, const Model& model,
                                              OracleWork& work) {
    const auto baseline = flow400_geometry(grid, model, work);
    flow400_require(baseline.empty(),
                    "Flow folded shape-data physical negative needs complete good baseline");
    work.spend(128u * sizeof(ShapeFrame) + 4096u);
    const auto contours = closed_frames(grid, work);
    auto node = std::ranges::find_if(
        model.nodes, [](const Node& value) { return value.shape != Shape::Rectangle; });
    if (node == model.nodes.end()) node = model.nodes.begin();
    flow400_require(node != model.nodes.end(),
                    "Flow folded shape-data physical negative lacks node");
    const auto frame = flow_typed_frame_for_node(grid, contours, *node, work);
    flow400_require(frame.has_value(), "Flow folded shape-data unique selected contour");
    if (node->shape == Shape::Decision) {
        const Point shoulder{frame->left + 1, frame->top + 1};
        flow_typed_expect_rejected(
            grid, model, work, [&](Grid& bad) { bad.put(shoulder, " "); },
            "Flow folded shape-data missing decision diagonal");
    } else if (node->shape == Shape::Rounded) {
        flow_typed_expect_rejected(
            grid, model, work, [&](Grid& bad) { bad.put({frame->left, frame->top}, "┌"); },
            "Flow folded shape-data mixed rounded corner");
    } else {
        flow_typed_expect_rejected(
            grid, model, work, [&](Grid& bad) { bad.put({frame->left, frame->top}, "╭"); },
            "Flow folded shape-data mixed rectangle corner");
    }
}

TEST(MermaidFlowFoldedShapeData, CompleteModelsCoverIndependentAndCombinedEnvelopeGrammar) {
    const auto asset = flow_folded_shape_data_documents();
    const auto documents = flow400_documents();
    const auto originals = documents.compact.root().get("cases"),
               fixtures = documents.manifest.root().get("fixtures");
    json::JsonMutDoc report;
    auto root = report.object(), rows = root.make_arr();
    std::size_t complete = 0u, official_bound = 0u, resources = 0u, mutants = 0u;
    asset.root().get("controls").iter([&](JsonVal item) {
        OracleWork work{kFlow400SceneLimit};
        // Scene storage, five full JSON frames, Models, contours, mutation copies, and report rows
        // are prepaid before capture.
        work.spend(393216u + 48u * sizeof(Grid) + 48u * sizeof(Model) + 192u * sizeof(ShapeFrame) +
                   192u * sizeof(Label) + 192u * sizeof(LabelRegion) +
                   2048u * sizeof(json::JsonMutVal));
        const auto id = flow400_text(item.get("id")),
                   expected_source = flow400_text(item.get("source"));
        const auto identity = item.get("official");
        const bool is_official = !identity.is_null();
        std::string source_owned;
        std::string_view source = expected_source;
        JsonVal original, fixture;
        if (is_official) {
            const auto ordinal = flow400_number(identity.get("ordinal")),
                       global = flow400_number(identity.get("manifest_global_ordinal"));
            original = originals.at(ordinal);
            fixture = fixtures.at(global);
            const auto fixture_path = flow400_text(fixture.get("path"));
            source_owned =
                flow400_read(std::filesystem::path(LOOM_TESTS_DIR) /
                             "fixtures/mermaid_official/cases" / std::string(fixture_path));
            source = source_owned;
        }
        auto row = root.make_obj();
        std::array<std::optional<Grid>, 5> frames;
        std::array<char, 257> error{};
        bool success = false, resource = false, decoded = false, source_wide = false,
             source_narrow = false, restored = false;
        std::size_t published = 0u, rejected = 0u;
        const char* phase = "capture";
        try {
            work.product(source.size(), 32u);
            auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
            element->ComputeRequirement();
            ordinary->ComputeRequirement();
            const int height =
                std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8,
                          static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 12});
            flow400_require(height <= 1040, "Flow folded shape-data complete source height");
            constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            constexpr std::array<std::string_view, 5> names{
                {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
            for (std::size_t n = 0u; n < frames.size(); ++n) {
                frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
                flow400_publication(*frames[n], work);
                flow400_append_frame(row, names[n], *frames[n]);
                ++published;
            }
            flow400_require(published == 5u, "Flow folded shape-data frames attached before bind");
            phase = "bind";
            work.product(source.size(), 4u);
            flow400_require(source == expected_source &&
                                flow400_number(item.get("source_bytes")) == source.size() &&
                                digest(source) == flow400_text(item.get("source_sha256")),
                            "Flow folded shape-data complete source identity");
            if (is_official) {
                const auto ordinal = flow400_number(identity.get("ordinal")),
                           global = flow400_number(identity.get("manifest_global_ordinal"));
                flow400_bind(original, fixture, ordinal, global, source, work);
                flow400_require(flow400_text(identity.get("fixture")) ==
                                        flow400_text(fixture.get("path")) &&
                                    flow400_text(identity.get("source_git_blob")) ==
                                        flow400_text(fixture.get("git_blob")),
                                "Flow folded shape-data official identity");
                flow400_require(original.get("model_json").is_null() &&
                                    original.get("model_sha256").is_null() &&
                                    original.get("adaptation").is_null(),
                                "Flow folded shape-data original remains pending");
                ++official_bound;
            }
            for (const auto& frame : frames)
                static_cast<void>(flow400_following(*frame, work));
            source_wide = flow400_same(*frames[0], *frames[3], work);
            source_narrow = flow400_same(*frames[1], *frames[4], work);
            restored = flow400_same(*frames[0], *frames[2], work);
            flow400_require(!source_wide, "Flow folded shape-data source fallback at wide width");
            flow400_require(restored, "Flow folded shape-data restored frame differs");
            phase = "model";
            const auto model_bytes = flow400_text(item.get("model_json"));
            auto document = json::parse(model_bytes);
            flow400_require(document.has_value() &&
                                digest(model_bytes) == flow400_text(item.get("model_sha256")),
                            "Flow folded shape-data Model JSON/SHA");
            const auto model = flow_typed_shape_model(document->root(), work);
            decoded = true;
            flow400_require(!model.nodes.empty() && !model.edges.empty(),
                            "Flow folded shape-data complete Model cardinality");
            phase = "geometry";
            const auto wide_errors = flow400_geometry(*frames[0], model, work);
            flow400_require(wide_errors.empty(),
                            wide_errors.empty() ? "" : wide_errors.front().c_str());
            if (!source_narrow) {
                const auto narrow_errors = flow400_geometry(*frames[1], model, work);
                flow400_require(narrow_errors.empty(),
                                narrow_errors.empty() ? "" : narrow_errors.front().c_str());
            }
            const auto restored_errors = flow400_geometry(*frames[2], model, work);
            flow400_require(restored_errors.empty(),
                            restored_errors.empty() ? "" : restored_errors.front().c_str());
            phase = "good_first_mutations";
            const auto required =
                2u +
                ((!model.edges.empty() && model.edges.front().from != model.edges.front().to)
                     ? 1u
                     : 0u) +
                (std::ranges::any_of(model.groups,
                                     [](const Group& group) { return !group.nodes.empty(); })
                     ? 1u
                     : 0u);
            rejected = flow400_mutations(*frames[0], model, work);
            flow400_require(rejected == required,
                            "Flow folded shape-data exact legacy mutation applicability");
            const auto shaped = std::ranges::find_if(
                model.nodes, [](const Node& node) { return node.shape != Shape::Rectangle; });
            const auto& selected = shaped == model.nodes.end() ? model.nodes.front() : *shaped;
            const Shape wrong =
                selected.shape == Shape::Rectangle ? Shape::Rounded : Shape::Rectangle;
            flow_typed_expect_model_rejected(
                *frames[0], model, work,
                [&](Model& bad) {
                    const auto found = std::ranges::find_if(
                        bad.nodes, [&](const Node& node) { return node.id == selected.id; });
                    flow400_require(found != bad.nodes.end(),
                                    "Flow folded shape-data selected Model node");
                    found->shape = wrong;
                },
                "Flow folded shape-data semantic shape identity");
            ++rejected;
            flow_folded_shape_data_physical_negative(*frames[0], model, work);
            ++rejected;
            flow400_require(rejected == required + 2u,
                            "Flow folded shape-data exact good-first negative count");
            success = true;
            phase = "complete";
        } catch (const std::exception& failure) {
            const char* message = failure.what();
            resource = work.remaining == 0u || (message && flow400_resource(message));
            for (std::size_t n = 0u; message && message[n] && n < 256u; ++n)
                error[n] = message[n];
        }
        row.set("id", id);
        row.set("official_source", is_official);
        row.set("published_frames", static_cast<int64_t>(published));
        row.set("typed_Model_decoded", decoded);
        row.set("source_wide", source_wide);
        row.set("source_narrow", source_narrow);
        row.set("restored_identically", restored);
        row.set("good_first_non_resource_rejections", static_cast<int64_t>(rejected));
        row.set("resource_error", resource);
        row.set("complete", success);
        row.set("phase", phase);
        row.set("error", std::string_view(error.data()));
        row.set("remaining", static_cast<int64_t>(work.remaining));
        rows.append(row);
        complete += success;
        resources += resource;
        mutants += rejected;
        EXPECT_TRUE(success) << id << ": " << std::string_view(error.data());
        EXPECT_FALSE(resource) << id;
        EXPECT_EQ(published, 5u) << id;
    });
    root.set("kind", "flow_folded_shape_data_native_full_model/v1");
    root.set("qualification",
             "Six Native controls prove folded description-only and closed node shape-data "
             "grammar. Two complete official originals bind their immutable source identities; no "
             "Flow400, Official, terminal, or foreign credit is claimed.");
    root.set("controls", 6);
    root.set("official_full_sources", 2);
    root.set("official_bound", static_cast<int64_t>(official_bound));
    root.set("complete", static_cast<int64_t>(complete));
    root.set("meaningful_mutants_rejected", static_cast<int64_t>(mutants));
    root.set("resource_errors", static_cast<int64_t>(resources));
    root.set("original_corpus_credit", 0);
    root.set("official_credit", 0);
    root.set("terminal_credit", 0);
    root.add("cases", rows);
    report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    std::filesystem::create_directories(LOOM_MERMAID_REPORT_DIR);
    std::ofstream output(std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                             "mermaid-flow-folded-shape-data-native.json",
                         std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n'));
#endif
    EXPECT_EQ(complete, 6u);
    EXPECT_EQ(official_bound, 2u);
    EXPECT_EQ(resources, 0u);
    EXPECT_GT(mutants, 0u);
}

struct FlowFoldedShapeDataClosed {
    std::string_view id, source, sha;
};

constexpr std::array<FlowFoldedShapeDataClosed, 8> kFlowFoldedShapeDataClosed{{
    {"folded_empty_scalar", R"FLOW(---
description: >-
---
flowchart LR
A --> B
)FLOW",
     "372c8d0ff38c67b8f647c83e62e3b05018302ce549a6253d79fd56a30da9ea4a"},
    {"folded_unknown_root", R"FLOW(---
description: >-
  Flow.
unknown: true
---
flowchart LR
A --> B
)FLOW",
     "d5825436cabd17386a4d5b7b65412e70319fc4bd00de21df573f8d01c0fe72a0"},
    {"folded_title", R"FLOW(---
description: >-
  Flow.
title: Visible
---
flowchart LR
A --> B
)FLOW",
     "498624c59d134deaa8d4e2ce90674f1f59a17e0d3b527df211d8788c02675098"},
    {"folded_empty_config", R"FLOW(---
description: >-
  Flow.
config:
---
flowchart LR
A --> B
)FLOW",
     "265d509ebd619ae9c35a0343b4e644c510ef6e330788d5b5eb7629b5fbcfb525"},
    {"folded_unapproved_layout", R"FLOW(---
description: >-
  Flow.
config:
  layout: elk
---
flowchart LR
A --> B
)FLOW",
     "6d57157fc6f73cdfc0573be6eb042d7fcef0f324a8e6378e8c01c5701c675fee"},
    {"shape_extra_key", R"FLOW(flowchart BT
  n2["Untitled Node"] --> n1@{ shape: diam, label: Nope}
)FLOW",
     "fc8aea7feb1cb020ed7b1e6ef2067e00daacd9d42f50b801cfcf8f4062056e25"},
    {"shape_quoted_token", R"FLOW(flowchart BT
  n2["Untitled Node"] --> n1@{ shape: "diam"}
)FLOW",
     "75ab59dc625688794c0b9a7b13711690bfe4deecb723ed608b0f277918d0bdbe"},
    {"shape_unclosed", R"FLOW(flowchart BT
  n2["Untitled Node"] --> n1@{ shape: diam
)FLOW",
     "e95dc530715270495154f654516343b917bb59228edf139533142e17df97670f"},
}};

TEST(MermaidFlowFoldedShapeData, MalformedEnvelopeAndUnlistedShapeDataStayFallback) {
    json::JsonMutDoc report;
    auto root = report.object(), rows = root.make_arr();
    std::size_t complete = 0u, resources = 0u;
    for (const auto& control : kFlowFoldedShapeDataClosed) {
        OracleWork work{kFlow400SceneLimit};
        work.spend(131072u + 32u * sizeof(Grid) + 512u * sizeof(json::JsonMutVal));
        auto row = root.make_obj();
        std::array<std::optional<Grid>, 5> frames;
        std::array<char, 257> error{};
        bool success = false, resource = false, source_wide = false, source_narrow = false;
        std::size_t published = 0u;
        const char* phase = "capture";
        try {
            flow400_require(digest(control.source) == control.sha,
                            "Flow folded shape-data closed source SHA");
            work.product(control.source.size(), 32u);
            auto element = loom::ui::render_markdown(fenced(control.source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(control.source, "text"));
            element->ComputeRequirement();
            ordinary->ComputeRequirement();
            const int height =
                std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
            flow400_require(height <= 1040, "Flow folded shape-data closed height");
            constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            constexpr std::array<std::string_view, 5> names{
                {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
            for (std::size_t n = 0u; n < frames.size(); ++n) {
                frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
                flow400_publication(*frames[n], work);
                flow400_append_frame(row, names[n], *frames[n]);
                ++published;
            }
            phase = "fallback";
            for (const auto& frame : frames)
                static_cast<void>(flow400_following(*frame, work));
            source_wide = flow400_same(*frames[0], *frames[3], work);
            source_narrow = flow400_same(*frames[1], *frames[4], work);
            flow400_require(source_wide && source_narrow,
                            "Flow folded shape-data closed source unexpectedly drawn");
            success = true;
            phase = "complete";
        } catch (const std::exception& failure) {
            const char* message = failure.what();
            resource = work.remaining == 0u || (message && flow400_resource(message));
            for (std::size_t n = 0u; message && message[n] && n < 256u; ++n)
                error[n] = message[n];
        }
        row.set("id", control.id);
        row.set("published_frames", static_cast<int64_t>(published));
        row.set("source_wide", source_wide);
        row.set("source_narrow", source_narrow);
        row.set("resource_error", resource);
        row.set("complete", success);
        row.set("phase", phase);
        row.set("error", std::string_view(error.data()));
        row.set("remaining", static_cast<int64_t>(work.remaining));
        rows.append(row);
        complete += success;
        resources += resource;
        EXPECT_TRUE(success) << control.id << ": " << std::string_view(error.data());
        EXPECT_FALSE(resource) << control.id;
        EXPECT_EQ(published, 5u) << control.id;
    }
    root.set("kind", "flow_folded_shape_data_closed_native/v1");
    root.set("qualification",
             "Malformed folded metadata and every unlisted local shape-data form remain fallback. "
             "The supported diamond alias has a separate positive geometry test; fallback controls "
             "earn no drawing admission.");
    root.set("controls", static_cast<int64_t>(kFlowFoldedShapeDataClosed.size()));
    root.set("complete", static_cast<int64_t>(complete));
    root.set("resource_errors", static_cast<int64_t>(resources));
    root.set("drawing_credit", 0);
    root.add("cases", rows);
    report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    std::filesystem::create_directories(LOOM_MERMAID_REPORT_DIR);
    std::ofstream output(std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                             "mermaid-flow-folded-shape-data-closed.json",
                         std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n'));
#endif
    EXPECT_EQ(complete, kFlowFoldedShapeDataClosed.size());
    EXPECT_EQ(resources, 0u);
}
}  // namespace

namespace {
struct FlowGoParallelPredicates {
    bool no_merge = false, labels = false, distinct = false;

    bool passed() const { return no_merge && labels && distinct; }
};

FlowGoParallelPredicates flow_go_parallel_predicates(const Grid& grid, JsonVal control,
                                                     OracleWork& work) {
    work.spend(8192u + 16u * sizeof(std::string));
    work.product(static_cast<std::size_t>(grid.width) * static_cast<std::size_t>(grid.height), 32u);
    std::string output;
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(grid.height));
    for (int y = 0; y < grid.height; ++y) {
        lines.push_back(grid.row(y));
        output += lines.back();
        output += '\n';
    }
    FlowGoParallelPredicates result;
    result.no_merge =
        output.find(flow400_text(control.get("forbidden_merge"))) == std::string::npos;
    result.labels = true;
    control.get("required_labels").iter([&](JsonVal label) {
        result.labels = result.labels && output.find(flow400_text(label)) != std::string::npos;
    });
    result.distinct = true;
    if (control.get("require_distinct_rows").as_bool()) {
        const auto labels = control.get("distinct_row_labels");
        flow400_require(labels.is_arr() && labels.size() == 2u, "Go parallel exact row-label pair");
        int first = -1, second = -1;
        for (std::size_t y = 0; y < lines.size(); ++y) {
            if (lines[y].find(flow400_text(labels.at(0u))) != std::string::npos)
                first = static_cast<int>(y);
            if (lines[y].find(flow400_text(labels.at(1u))) != std::string::npos)
                second = static_cast<int>(y);
        }
        result.distinct = first != -1 && second != -1 && first != second;
    }
    return result;
}

TEST(MermaidAsciiFlowAssertions, ParallelCaptionsRetainCompleteOriginalPortablePredicates) {
    OracleWork setup;
    setup.spend(65536u + 16u * sizeof(json::JsonDoc));
    setup.product(4930u, 32u);
    const auto asset_path = std::filesystem::path(LOOM_TESTS_DIR) /
                            "fixtures/mermaid_native/flow_go_parallel_portable_assertions.v1.json";
    flow400_require(std::filesystem::file_size(asset_path) == 4930u,
                    "Go parallel asset byte bound before read");
    const auto raw = flow400_read(asset_path);
    flow400_require(raw.size() == 4930u &&
                        digest(raw) ==
                            "fe3a05c2ef8f041d4958a0d11eff08418dc9b27541eb15d32ed1faaaf68c42f8",
                    "Go parallel immutable asset");
    auto asset = json::parse(raw);
    ASSERT_TRUE(asset.has_value());
    const auto controls = asset->root().get("cases");
    ASSERT_EQ(controls.size(), 3u);
    json::JsonMutDoc report;
    auto root = report.object(), rows = root.make_arr();
    std::size_t complete = 0u, predicate_groups = 0u, predicate_negatives = 0u,
                geometry_negatives = 0u, resources = 0u;
    controls.iter([&](JsonVal control) {
        OracleWork work{kFlow400SceneLimit};
        // The single scene ledger pays five frames, source/Model provenance,
        // report rows, predicate output strings, and later paid negative copies.
        work.spend(393216u + 48u * sizeof(Grid) + 32u * sizeof(Model) +
                   512u * sizeof(json::JsonMutVal));
        const auto source = flow400_text(control.get("source"));
        auto row = root.make_obj();
        std::array<std::optional<Grid>, 5> frames;
        std::array<char, 257> error{};
        std::size_t published = 0u, direct = 0u, negative = 0u, geometry = 0u;
        bool success = false, resource = false, source_narrow = false;
        const char* phase = "capture";
        try {
            work.product(source.size(), 32u);
            auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
            auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
            element->ComputeRequirement();
            ordinary->ComputeRequirement();
            const int height =
                std::max({32, element->requirement().min_y + 8, ordinary->requirement().min_y + 8});
            flow400_require(height <= 1040, "Go parallel source height");
            constexpr std::array<int, 5> widths{{240, 40, 240, 240, 40}};
            constexpr std::array<std::string_view, 5> names{
                {"wide", "narrow", "restored", "ordinary_wide", "ordinary_narrow"}};
            for (std::size_t n = 0; n < frames.size(); ++n) {
                frames[n] = capture(n < 3u ? element : ordinary, widths[n], height, &work);
                flow400_publication(*frames[n], work);
                flow400_append_frame(row, names[n], *frames[n]);
                ++published;
            }
            flow400_require(published == 5u, "Go parallel five frames attached before bind");
            phase = "bind";
            work.product(9710u, 16u);
            const auto upstream_path =
                std::filesystem::path(LOOM_TESTS_DIR).parent_path() /
                std::string(flow400_text(asset->root().get("upstream_path")));
            flow400_require(std::filesystem::file_size(upstream_path) == 9710u,
                            "Go parallel upstream byte bound before read");
            const auto original = flow400_read(upstream_path);
            flow400_require(original.size() == 9710u &&
                                digest(original) ==
                                    flow400_text(asset->root().get("upstream_sha256")),
                            "Go parallel full upstream source identity");
            const auto begin = flow400_number(control.get("function_byte_start")),
                       end = flow400_number(control.get("function_byte_end"));
            flow400_require(begin < end && end <= original.size() &&
                                digest(std::string_view(original).substr(begin, end - begin)) ==
                                    flow400_text(control.get("function_sha256")),
                            "Go parallel complete function span");
            flow400_require(digest(source) == flow400_text(control.get("source_sha256")),
                            "Go parallel complete input identity");
            for (const auto& frame : frames)
                static_cast<void>(flow400_following(*frame, work));
            flow400_require(!flow400_same(*frames[0], *frames[3], work),
                            "Go parallel mandatory wide returned source");
            source_narrow = flow400_same(*frames[1], *frames[4], work);
            flow400_require(flow400_same(*frames[0], *frames[2], work),
                            "Go parallel exact restored frame");
            work.spend(4096u + 8u * sizeof(std::string));
            work.product(source.size() + 32u, 32u);
            std::string literal = "\"";
            for (const auto byte : source) {
                switch (byte) {
                case '\n':
                    literal += "\\n";
                    break;
                case '\r':
                    literal += "\\r";
                    break;
                case '\t':
                    literal += "\\t";
                    break;
                case '\\':
                    literal += "\\\\";
                    break;
                case '"':
                    literal += "\\\"";
                    break;
                default:
                    flow400_require(static_cast<unsigned char>(byte) >= 32u &&
                                        static_cast<unsigned char>(byte) < 127u,
                                    "Go parallel closed ASCII literal byte");
                    literal += byte;
                }
            }
            literal += '"';
            const auto invocation = "renderGraph(" + literal + ", config)";
            const auto function = std::string_view(original).substr(begin, end - begin);
            flow400_require(function.find(invocation) != std::string_view::npos,
                            "Go parallel original renderGraph literal/input identity");
            phase = "model";
            const auto model_bytes = flow400_text(control.get("model_json"));
            work.product(model_bytes.size(), 32u);
            work.spend(8192u + 4u * sizeof(json::JsonDoc));
            flow400_require(digest(model_bytes) == flow400_text(control.get("model_sha256")),
                            "Go parallel source-authored Model SHA");
            auto document = json::parse(model_bytes);
            flow400_require(document.has_value(), "Go parallel source-authored Model JSON");
            const auto model = flow_typed_shape_model(document->root(), work);
            flow400_require(model.nodes.size() == 2u && model.edges.size() == 2u &&
                                model.groups.empty(),
                            "Go parallel full Model cardinality");
            phase = "geometry";
            for (const auto index : {0u, 1u, 2u}) {
                if (index == 1u && source_narrow) continue;
                const auto errors = flow400_geometry(*frames[index], model, work);
                flow400_require(errors.empty(), errors.empty() ? "" : errors.front().c_str());
                flow400_require(flow_go_parallel_predicates(*frames[index], control, work).passed(),
                                "Go parallel original portable predicates failed");
            }
            direct = control.get("require_distinct_rows").as_bool() ? 3u : 2u;
            flow400_require(
                direct == flow400_number(control.get("portable_semantic_predicate_callsites")),
                "Go parallel exact predicate-group count");
            phase = "good_first_mutations";
            geometry = flow400_mutations(*frames[0], model, work);
            flow400_require(geometry == 3u, "Go parallel exact legacy geometry negatives");
            const auto& good = *frames[0];
            const auto first_label = std::string_view(model.edges.front().label),
                       second_label = std::string_view(model.edges.back().label);
            const auto first = flow_typed_caption_blocks(good, first_label, work),
                       second = flow_typed_caption_blocks(good, second_label, work);
            flow400_require(first.size() == 1u && second.size() == 1u,
                            "Go parallel unique complete captions for mutations");
            {
                flow400_copy_grid(good, work);
                auto bad = good;
                flow_typed_erase_label(bad, first.front(), first_label, work);
                flow400_require(!flow_go_parallel_predicates(bad, control, work).labels,
                                "Go parallel required-label predicate missed erased caption");
                ++negative;
            }
            {
                flow400_copy_grid(good, work);
                auto bad = good;
                flow_typed_write_at(bad, {180, 0}, flow400_text(control.get("forbidden_merge")),
                                    work);
                flow400_require(!flow_go_parallel_predicates(bad, control, work).no_merge,
                                "Go parallel anti-merge predicate missed merged caption");
                ++negative;
            }
            if (control.get("require_distinct_rows").as_bool()) {
                flow400_copy_grid(good, work);
                auto bad = good;
                flow_typed_erase_label(bad, second.front(), second_label, work);
                flow_typed_write_at(bad, {180, first.front().starts.front().y}, second_label, work);
                const auto result = flow_go_parallel_predicates(bad, control, work);
                flow400_require(result.no_merge && result.labels && !result.distinct,
                                "Go parallel same-row predicate missed caption relocation");
                ++negative;
            }
            flow400_require(negative == direct,
                            "Go parallel every portable predicate has a direct negative");
            success = true;
            phase = "complete";
        } catch (const std::exception& failure) {
            const char* message = failure.what();
            resource = work.remaining == 0u || (message && flow400_resource(message));
            for (std::size_t n = 0; message && message[n] && n < 256u; ++n)
                error[n] = message[n];
        }
        row.set("case_id", flow400_text(control.get("case_id")));
        row.set("source_sha256", flow400_text(control.get("source_sha256")));
        row.set("published_frames", static_cast<int64_t>(published));
        row.set("source_narrow", source_narrow);
        row.set("complete", success);
        row.set("resource_error", resource);
        row.set("portable_predicate_groups", static_cast<int64_t>(direct));
        row.set("predicate_negatives", static_cast<int64_t>(negative));
        row.set("geometry_negatives", static_cast<int64_t>(geometry));
        row.set("phase", phase);
        row.set("error", std::string_view(error.data()));
        row.set("remaining", static_cast<int64_t>(work.remaining));
        rows.append(row);
        complete += success;
        predicate_groups += direct;
        predicate_negatives += negative;
        geometry_negatives += geometry;
        resources += resource;
        EXPECT_TRUE(success) << flow400_text(control.get("case_id")) << ": " << error.data();
        EXPECT_FALSE(resource);
        EXPECT_EQ(published, 5u);
    });
    root.set("schema", "flow_go_parallel_portable_assertion_results/v1");
    root.set("qualification",
             "Eight portable conditional groups from three source-bound Go functions only; true "
             "ASCII presentation options, Go result API, complete foreign functions and five-suite "
             "semantic completion remain unproved. Source fallback is never drawing success.");
    root.set("complete_sources", static_cast<int64_t>(complete));
    root.set("portable_predicate_groups", static_cast<int64_t>(predicate_groups));
    root.set("predicate_negatives", static_cast<int64_t>(predicate_negatives));
    root.set("geometry_negatives", static_cast<int64_t>(geometry_negatives));
    root.set("resource_errors", static_cast<int64_t>(resources));
    root.set("formal_admissions", 0);
    root.add("cases", rows);
    report.set_root(root);
#ifdef LOOM_MERMAID_REPORT_DIR
    std::ofstream output(std::filesystem::path(LOOM_MERMAID_REPORT_DIR) /
                             "mermaid-flow-go-parallel-portable-assertions.json",
                         std::ios::binary);
    ASSERT_TRUE(output && (output << report.to_string() << '\n'));
#endif
    EXPECT_EQ(complete, 3u);
    EXPECT_EQ(predicate_groups, 8u);
    EXPECT_EQ(predicate_negatives, 8u);
    EXPECT_EQ(geometry_negatives, 9u);
    EXPECT_EQ(resources, 0u);
}
}  // namespace

namespace {
TEST(MermaidFlowCaptionSearch, FullGridIncludesEveryBoundaryCell) {
    const auto grid = mock({"top    Q", "       R", "bottom S"});
    OracleWork work;
    const auto expect = [&](std::string_view text, Point expected) {
        const auto blocks = flow_typed_caption_blocks(grid, text, work);
        ASSERT_EQ(blocks.size(), 1u) << text;
        ASSERT_EQ(blocks.front().starts.size(), 1u) << text;
        EXPECT_EQ(blocks.front().starts.front(), expected) << text;
    };
    expect("top", {0, 0});
    expect("Q", {7, 0});
    expect("R", {7, 1});
    expect("bottom", {0, 2});
    expect("S", {7, 2});
    EXPECT_TRUE(flow_typed_caption_blocks(grid, "", work).empty());
    EXPECT_TRUE(flow_typed_caption_blocks(grid, "ghost", work).empty());
    EXPECT_TRUE(flow_typed_caption_blocks(grid, "bot", work).empty());
}
}  // namespace

namespace {
TEST(MermaidFiveSuitePartialAssertionIndex,
     PinnedExpressionsAndUnresolvedBoundariesMatchTrackedClosure) {
    constexpr std::string_view asset = "fixtures/mermaid_terminal/assertion_index/v3/"
                                       "five_suite_partial_source_assertion_index.json";
    constexpr std::string_view asset_sha =
        "4270e835f1472566fd8b3dcadb223ccdc7df341d6358d7336207c259fe389351";
    constexpr std::size_t asset_bytes = 6149870u;
    constexpr std::string_view closure_repo_path =
        "tests/fixtures/mermaid_terminal/assertion_index/v3/input-closure.json";
    constexpr std::string_view closure_sha =
        "2d7a7119ceb3c7a3898e5f9fb2ca06032f8193a052e13fd54e093db3cc6d55b0";
    constexpr std::size_t closure_bytes = 1447u;
    constexpr std::size_t expected_files = 861u, expected_containers = 1429u,
                          expected_direct = 2573u, expected_unresolved = 400u,
                          expected_partial_helpers = 87u, expected_summary_only = 186u,
                          expected_records = 3060u, expected_capture_origins = 1400u,
                          expected_file_origins = 1173u;
    const auto root_path = std::filesystem::path(LOOM_TESTS_DIR),
               repository = root_path.parent_path(), asset_path = root_path / asset;
    ASSERT_EQ(std::filesystem::file_size(asset_path), asset_bytes);
    const auto text = flow400_read(asset_path);
    ASSERT_EQ(digest(text), asset_sha);
    const auto document = json::parse(text);
    ASSERT_TRUE(document.has_value());
    const auto ledger = document->root();
    flow400_keys(ledger, {"schema", "status", "scope", "inputs", "source_files", "containers",
                          "records", "summary"});
    ASSERT_EQ(flow400_text(ledger.get("schema")), "five_suite_partial_source_assertion_index/v3");
    ASSERT_EQ(flow400_text(ledger.get("status")),
              "static_source_index_not_execution_or_port_completion");
    const auto scope = ledger.get("scope");
    flow400_keys(scope, {"frozen_source_containers", "direct_assertion_records",
                         "unresolved_source_containers", "unresolved_partial_helpers",
                         "summary_only_provenance_records", "runtime_expansion_rule",
                         "semantic_assertion_pass_credit", "runtime_expansion_credit",
                         "coverage_statement"});
    ASSERT_EQ(flow400_number(scope.get("frozen_source_containers")), expected_containers);
    ASSERT_EQ(flow400_number(scope.get("direct_assertion_records")), expected_direct);
    ASSERT_EQ(flow400_number(scope.get("unresolved_source_containers")), expected_unresolved);
    ASSERT_EQ(flow400_number(scope.get("unresolved_partial_helpers")), expected_partial_helpers);
    ASSERT_EQ(flow400_number(scope.get("summary_only_provenance_records")), expected_summary_only);
    ASSERT_EQ(flow400_number(scope.get("semantic_assertion_pass_credit")), 0);
    ASSERT_EQ(flow400_number(scope.get("runtime_expansion_credit")), 0);
    ASSERT_EQ(flow400_text(scope.get("runtime_expansion_rule")),
              "A case binding is never represented as a runtime expansion count without a "
              "source-specific execution proof.");
    ASSERT_EQ(flow400_text(scope.get("coverage_statement")),
              "Partial index of the frozen input ledgers only; it is not a five-suite "
              "semantic-assertion denominator or a port-completeness claim.");
    const auto inputs = ledger.get("inputs");
    flow400_keys(inputs, {"input_closure_path", "input_closure_sha256",
                          "tracked_source_input_ledgers", "source_file_closure", "extractor"});
    ASSERT_EQ(flow400_text(inputs.get("input_closure_path")), closure_repo_path);
    ASSERT_EQ(flow400_text(inputs.get("input_closure_sha256")), closure_sha);
    ASSERT_EQ(flow400_text(inputs.get("source_file_closure")),
              "Every referenced source file is pinned in source_files and must be present in the "
              "checkout at its listed path and hash.");
    const auto closure_path = repository / closure_repo_path;
    ASSERT_EQ(std::filesystem::file_size(closure_path), closure_bytes);
    const auto closure_text = flow400_read(closure_path);
    ASSERT_EQ(digest(closure_text), closure_sha);
    const auto closure_document = json::parse(closure_text);
    ASSERT_TRUE(closure_document.has_value());
    const auto closure = closure_document->root();
    flow400_keys(closure, {"schema", "status", "input_ledgers", "captured_source_slice_rule",
                           "source_file_closure"});
    ASSERT_EQ(flow400_text(closure.get("schema")), "five_suite_assertion_input_closure/v1");
    ASSERT_EQ(flow400_text(closure.get("status")),
              "static_source_inputs_only_not_execution_or_port_completion");
    ASSERT_EQ(flow400_text(closure.get("source_file_closure")),
              "The generated index pins every upstream source file separately in source_files; "
              "this closure pins only the two reviewed source-ledger inputs.");
    const auto capture_rule = closure.get("captured_source_slice_rule");
    flow400_keys(capture_rule, {"original_kind", "required_equal_fields", "required_file_identity",
                                "accepted_container_count", "meaning"});
    ASSERT_EQ(flow400_text(capture_rule.get("original_kind")),
              "exact_original_test_function_slice");
    ASSERT_EQ(flow400_number(capture_rule.get("accepted_container_count")), 533);
    ASSERT_EQ(flow400_text(capture_rule.get("meaning")),
              "Only these explicitly proven runtime-capture files map local parser intervals back "
              "to upstream offsets; equal byte length alone is insufficient.");
    std::set<std::string> equal_fields, file_identity;
    capture_rule.get("required_equal_fields").iter([&](JsonVal field) {
        equal_fields.emplace(flow400_text(field));
    });
    capture_rule.get("required_file_identity").iter([&](JsonVal field) {
        file_identity.emplace(flow400_text(field));
    });
    EXPECT_EQ(equal_fields, (std::set<std::string>{"original.byte_start=source.byte_start",
                                                   "original.byte_end=source.byte_end"}));
    EXPECT_EQ(file_identity,
              (std::set<std::string>{"source.bytes=len(captured_file)",
                                     "source.sha256=sha256(captured_file)",
                                     "source.selected_bytes_sha256=sha256(captured_file)",
                                     "original.sha256=sha256(captured_file)"}));
    const auto closure_ledgers = closure.get("input_ledgers"),
               tracked_ledgers = inputs.get("tracked_source_input_ledgers");
    ASSERT_TRUE(closure_ledgers.is_arr() && tracked_ledgers.is_arr());
    ASSERT_EQ(closure_ledgers.size(), 2u);
    ASSERT_EQ(tracked_ledgers.size(), 2u);
    std::map<std::string, std::pair<std::string, std::size_t>> closure_rows;
    closure_ledgers.iter([&](JsonVal row) {
        flow400_keys(row, {"path", "sha256", "bytes"});
        const auto path = std::string(flow400_text(row.get("path")));
        ASSERT_TRUE(
            path.starts_with("tests/fixtures/mermaid_terminal/assertion_index/v3/source-inputs/") &&
            path.find("..") == std::string::npos);
        const auto bytes = flow400_number(row.get("bytes"));
        const auto raw = flow400_read(repository / path);
        ASSERT_EQ(raw.size(), bytes);
        ASSERT_EQ(digest(raw), flow400_text(row.get("sha256")));
        ASSERT_TRUE(
            closure_rows
                .emplace(path, std::make_pair(std::string(flow400_text(row.get("sha256"))), bytes))
                .second);
    });
    tracked_ledgers.iter([&](JsonVal row) {
        flow400_keys(row, {"path", "sha256", "bytes"});
        const auto path = std::string(flow400_text(row.get("path")));
        const auto found = closure_rows.find(path);
        ASSERT_NE(found, closure_rows.end());
        if (found != closure_rows.end()) {
            ASSERT_EQ(flow400_text(row.get("sha256")), found->second.first);
            ASSERT_EQ(flow400_number(row.get("bytes")), found->second.second);
        }
    });
    const auto source_files = ledger.get("source_files"), containers = ledger.get("containers"),
               records = ledger.get("records");
    ASSERT_TRUE(source_files.is_arr() && containers.is_arr() && records.is_arr());
    ASSERT_EQ(source_files.size(), expected_files);
    ASSERT_EQ(containers.size(), expected_containers);
    ASSERT_EQ(records.size(), expected_records);
    std::map<std::string, std::pair<std::string, std::size_t>> files;
    std::set<std::string> container_ids;
    source_files.iter([&](JsonVal file) {
        flow400_keys(file, {"path", "sha256", "bytes"});
        const auto path = std::string(flow400_text(file.get("path")));
        ASSERT_TRUE(path.starts_with("tests/fixtures/mermaid_terminal/") &&
                    path.find("..") == std::string::npos);
        const auto bytes = flow400_number(file.get("bytes"));
        const auto raw = flow400_read(repository / path);
        ASSERT_EQ(raw.size(), bytes);
        ASSERT_EQ(digest(raw), flow400_text(file.get("sha256")));
        ASSERT_TRUE(
            files
                .emplace(path, std::make_pair(std::string(flow400_text(file.get("sha256"))), bytes))
                .second);
    });
    containers.iter([&](JsonVal container) {
        flow400_keys(container,
                     {"id", "vendor", "source_path", "source_sha256", "case_bindings", "kind"});
        const auto id = std::string(flow400_text(container.get("id")));
        ASSERT_TRUE(container_ids.insert(id).second);
        const auto found = files.find(std::string(flow400_text(container.get("source_path"))));
        ASSERT_NE(found, files.end());
        if (found != files.end())
            ASSERT_EQ(found->second.first, flow400_text(container.get("source_sha256")));
        ASSERT_TRUE(container.get("case_bindings").is_arr());
    });
    std::size_t direct = 0u, unresolved = 0u, partial_helpers = 0u, summary_only = 0u,
                capture_origins = 0u, file_origins = 0u;
    records.iter([&](JsonVal record) {
        const auto type = flow400_text(record.get("record_type"));
        const auto source = record.get("source");
        const auto path = std::string(flow400_text(source.get("path")));
        const auto known = files.find(path);
        ASSERT_NE(known, files.end());
        if (known == files.end()) return;
        const auto raw = flow400_read(repository / path);
        const auto start = flow400_number(source.get("byte_start")),
                   end = flow400_number(source.get("byte_end"));
        ASSERT_LT(start, end);
        ASSERT_LE(end, raw.size());
        ASSERT_EQ(digest(raw), flow400_text(source.get("file_sha256")));
        if (start >= end || end > raw.size()) return;
        if (type == "direct_assertion") {
            ++direct;
            flow400_keys(record, {"id", "record_type", "vendor", "language", "assertion_form",
                                  "source", "source_origins", "case_bindings", "runtime_expansion",
                                  "native_status", "source_unit_ids"});
            flow400_keys(source, {"path", "file_sha256", "byte_start", "byte_end", "line_start",
                                  "line_end", "expression_sha256", "expression", "selection"});
            const auto expression = raw.substr(start, end - start);
            ASSERT_EQ(digest(expression), flow400_text(source.get("expression_sha256")));
            ASSERT_EQ(expression, flow400_text(source.get("expression")));
            ASSERT_NE(flow400_text(source.get("selection")),
                      "exact_preserved_expression_provenance_only");
            const auto origins = record.get("source_origins"),
                       units = record.get("source_unit_ids");
            ASSERT_TRUE(origins.is_arr() && origins.size() > 0u && units.is_arr() &&
                        units.size() > 0u);
            units.iter([&](JsonVal unit) {
                ASSERT_TRUE(container_ids.contains(std::string(flow400_text(unit))));
            });
            origins.iter([&](JsonVal origin) {
                const auto mapping = flow400_text(origin.get("mapping"));
                const auto origin_path = std::string(flow400_text(origin.get("path")));
                ASSERT_TRUE(!origin_path.empty() && !origin_path.starts_with("/") &&
                            origin_path.find("..") == std::string::npos);
                if (mapping == "captured_exact_source_slice_plus_local_byte_interval") {
                    ++capture_origins;
                    flow400_keys(origin,
                                 {"path", "captured_slice_byte_start", "captured_slice_byte_end",
                                  "assertion_byte_start", "assertion_byte_end", "mapping"});
                    const auto captured_start =
                                   flow400_number(origin.get("captured_slice_byte_start")),
                               captured_end = flow400_number(origin.get("captured_slice_byte_end")),
                               assertion_start = flow400_number(origin.get("assertion_byte_start")),
                               assertion_end = flow400_number(origin.get("assertion_byte_end"));
                    ASSERT_LT(captured_start, captured_end);
                    ASSERT_LE(captured_start, assertion_start);
                    ASSERT_LE(assertion_end, captured_end);
                    ASSERT_LT(assertion_start, assertion_end);
                    ASSERT_EQ(assertion_start - captured_start, start);
                    ASSERT_EQ(assertion_end - captured_start, end);
                } else if (mapping == "captured_file_byte_interval") {
                    ++file_origins;
                    flow400_keys(origin,
                                 {"path", "assertion_byte_start", "assertion_byte_end", "mapping"});
                    const auto assertion_start = flow400_number(origin.get("assertion_byte_start")),
                               assertion_end = flow400_number(origin.get("assertion_byte_end"));
                    ASSERT_LT(assertion_start, assertion_end);
                    ASSERT_EQ(assertion_start, start);
                    ASSERT_EQ(assertion_end, end);
                } else
                    ADD_FAILURE() << "unknown source-origin mapping: " << mapping;
            });
        } else {
            const bool is_partial = type == "unresolved_partial_helper";
            ASSERT_TRUE(type == "unresolved_source_container" || is_partial);
            if (is_partial)
                ++partial_helpers;
            else
                ++unresolved;
            flow400_keys(record, {"id", "record_type", "source_unit_id", "vendor", "language",
                                  "source", "case_bindings", "runtime_expansion",
                                  "helper_resolution", "native_status"});
            flow400_keys(source, {"path", "file_sha256", "byte_start", "byte_end", "line_start",
                                  "line_end", "selected_sha256", "selection", "test_name"});
            ASSERT_EQ(digest(raw.substr(start, end - start)),
                      flow400_text(source.get("selected_sha256")));
            ASSERT_TRUE(
                container_ids.contains(std::string(flow400_text(record.get("source_unit_id")))));
            const auto helper = record.get("helper_resolution");
            flow400_keys(helper, {"status", "names", "diagnostics"});
            ASSERT_EQ(flow400_text(helper.get("status")), "unresolved");
            ASSERT_TRUE(helper.get("names").is_arr() && helper.get("diagnostics").is_arr() &&
                        helper.get("diagnostics").size() > 0u);
            bool provenance_only = false;
            helper.get("diagnostics").iter([&](JsonVal diagnostic) {
                if (flow400_text(diagnostic) ==
                    "summary_label_is_provenance_only_requires_full_original_function")
                    provenance_only = true;
            });
            if (provenance_only) {
                ++summary_only;
                ASSERT_EQ(type, "unresolved_source_container");
                const auto vendor = flow400_text(record.get("vendor"));
                ASSERT_TRUE(vendor == "beautiful_mermaid" || vendor == "mermaid_ascii");
            }
        }
    });
    EXPECT_EQ(direct, expected_direct);
    EXPECT_EQ(unresolved, expected_unresolved);
    EXPECT_EQ(partial_helpers, expected_partial_helpers);
    EXPECT_EQ(summary_only, expected_summary_only);
    EXPECT_EQ(capture_origins, expected_capture_origins);
    EXPECT_EQ(file_origins, expected_file_origins);
}
}  // namespace

namespace {
TEST(MermaidFlowTypedFollowingToken, CallerTokenOwnsBothGoodAndMutatedChecks) {
    const Model model{"LR", {{"a", "A"}, {"b", "B"}}, {{0, 1, "", "solid", false, true}}, {}};
    constexpr std::string_view token = "AFTER_MERMAID";
    const auto good = mock({"┌───┐     ┌───┐         ", "│ A ├────▶│ B │         ",
                            "└───┘     └───┘         ", "", token, ""});
    OracleWork work{kFlow400SceneLimit};
    ASSERT_TRUE(flow400_geometry(good, model, work, token).empty());
    const auto erase_route = [](Grid& bad) { bad.put({7, 1}, " "); };
    const auto reverse_arrow = [](Model& bad) {
        bad.edges.front().start = true;
        bad.edges.front().end = false;
    };
    bool wrong_token = false;
    try {
        flow_typed_expect_rejected(good, model, work, erase_route, "wrong default token");
    } catch (const std::exception& error) {
        wrong_token =
            error.what() == std::string_view("Flow400 protocol exact independent following row");
    }
    EXPECT_TRUE(wrong_token);
    EXPECT_NO_THROW(flow_typed_expect_rejected(good, model, work, erase_route,
                                               "caller-token physical route", token));
    EXPECT_NO_THROW(flow_typed_expect_model_rejected(good, model, work, reverse_arrow,
                                                     "caller-token semantic arrow", token));
    OracleWork zero{0u};
    bool resource = false;
    try {
        flow_typed_expect_rejected(good, model, zero, erase_route, "caller-token resource", token);
    } catch (const std::exception& error) {
        resource = flow400_resource(error.what());
    }
    EXPECT_TRUE(resource);
}

TEST(MermaidFlowLinkProfile3Oracle, AllFourFacesKeepEndpointMarkersAndStrokeOwnership) {
    std::size_t goods = 0u, rejections = 0u;
    for (int face = 0; face < 4; ++face)
        for (const auto style : std::array<std::string_view, 3>{"solid", "dotted", "thick"})
            for (int markers = 0; markers < 3; ++markers) {
                SCOPED_TRACE(std::to_string(face) + "/" + std::string(style) + "/" +
                             std::to_string(markers));
                OracleWork work{kFlow400SceneLimit};
                work.spend(262144u);
                Grid good{40, 32, std::vector<std::string>(1280u, " ")};
                const auto put_text = [&](int x, int y, std::string_view text) {
                    for (const char ch : text)
                        good.put({x++, y}, std::string(1, ch));
                };
                const auto box = [&](int l, int t, int r, int b, std::string_view label) {
                    good.put({l, t}, "┌");
                    good.put({r, t}, "┐");
                    good.put({l, b}, "└");
                    good.put({r, b}, "┘");
                    for (int x = l + 1; x < r; ++x) {
                        good.put({x, t}, "─");
                        good.put({x, b}, "─");
                    }
                    for (int y = t + 1; y < b; ++y) {
                        good.put({l, y}, "│");
                        good.put({r, y}, "│");
                    }
                    put_text((l + r) / 2, (t + b) / 2, label);
                };
                std::vector<Point> path;
                Point source_border{}, target_border{};
                std::string_view source_open, target_open, source_plain, target_plain;
                if (face == 0) {
                    box(2, 2, 10, 6, "S");
                    box(18, 15, 26, 19, "T");
                    source_border = {10, 4};
                    target_border = {26, 17};
                    for (int x = 11; x <= 30; ++x)
                        path.push_back({x, 4});
                    for (int y = 5; y <= 17; ++y)
                        path.push_back({30, y});
                    for (int x = 29; x >= 27; --x)
                        path.push_back({x, 17});
                    source_open = target_open = "├";
                    source_plain = target_plain = "│";
                } else if (face == 1) {
                    box(22, 2, 30, 6, "S");
                    box(6, 15, 14, 19, "T");
                    source_border = {22, 4};
                    target_border = {6, 17};
                    for (int x = 21; x >= 2; --x)
                        path.push_back({x, 4});
                    for (int y = 5; y <= 17; ++y)
                        path.push_back({2, y});
                    for (int x = 3; x <= 5; ++x)
                        path.push_back({x, 17});
                    source_open = target_open = "┤";
                    source_plain = target_plain = "│";
                } else if (face == 2) {
                    box(2, 2, 10, 6, "S");
                    box(18, 15, 26, 19, "T");
                    source_border = {6, 2};
                    target_border = {22, 19};
                    path.push_back({6, 1});
                    path.push_back({6, 0});
                    for (int x = 7; x <= 31; ++x)
                        path.push_back({x, 0});
                    for (int y = 1; y <= 25; ++y)
                        path.push_back({31, y});
                    for (int x = 30; x >= 22; --x)
                        path.push_back({x, 25});
                    for (int y = 24; y >= 20; --y)
                        path.push_back({22, y});
                    source_open = "┴";
                    target_open = "┬";
                    source_plain = target_plain = "─";
                } else {
                    box(2, 2, 10, 6, "S");
                    box(2, 15, 10, 19, "T");
                    source_border = {6, 6};
                    target_border = {6, 15};
                    for (int y = 7; y <= 14; ++y)
                        path.push_back({6, y});
                    source_open = "┬";
                    target_open = "┴";
                    source_plain = target_plain = "─";
                }
                const bool source_point = markers == 2, target_point = markers != 0;
                if (!source_point) good.put(source_border, std::string(source_open));
                if (!target_point) good.put(target_border, std::string(target_open));
                const auto arm = [](Point at, Point other) {
                    if (other.x < at.x) return L;
                    if (other.x > at.x) return R;
                    return other.y < at.y ? U : D;
                };
                const auto toward = [](Point at, Point other) -> std::string_view {
                    if (other.x < at.x) return "◀";
                    if (other.x > at.x) return "▶";
                    return other.y < at.y ? "▲" : "▼";
                };
                const auto stroke = [&](int bits) -> std::string_view {
                    if (bits == (L | R))
                        return style == "solid" ? "─" : style == "dotted" ? "┄" : "━";
                    if (bits == (U | D))
                        return style == "solid" ? "│" : style == "dotted" ? "┆" : "┃";
                    if (bits == (R | D)) return "┌";
                    if (bits == (L | D)) return "┐";
                    if (bits == (L | U)) return "┘";
                    if (bits == (R | U)) return "└";
                    throw std::runtime_error(
                        "Independent four-face hand route is not a single orthogonal path");
                };
                for (std::size_t at = 0u; at < path.size(); ++at) {
                    const auto before = at ? path[at - 1u] : source_border,
                               after = at + 1u < path.size() ? path[at + 1u] : target_border;
                    good.put(
                        path[at],
                        std::string(source_point && at == 0u ? toward(path[at], source_border)
                                    : target_point && at + 1u == path.size()
                                        ? toward(path[at], target_border)
                                        : stroke(arm(path[at], before) | arm(path[at], after))));
                }
                put_text(0, 31, kSentinel);
                const Model model{"TD",
                                  {{"S", "S"}, {"T", "T"}},
                                  {{0, 1, "", std::string(style), source_point, target_point}},
                                  {}};
                const LinkProfile profile{
                    {{source_point ? LinkMarker::Point : LinkMarker::None,
                      target_point ? LinkMarker::Point : LinkMarker::None, 1}},
                    {0, 1}};
                const auto errors = flow_linkprofile3_geometry(good, model, profile, work);
                ASSERT_TRUE(errors.empty()) << (errors.empty() ? "" : errors.front());
                ++goods;
                if (face == 3 && style == "solid" && markers == 1) {
                    // A shared baseline must reject a broken good before any callback,
                    // and neither a no-op nor an empty ledger can earn negative credit.
                    work.spend(8192u);
                    flow400_copy_grid(good, work);
                    auto broken = good;
                    broken.put(path.front(), " ");
                    bool called = false;
                    std::string failure;
                    try {
                        LinkProfileMutationScene(broken, model, profile, work)
                            .physical([&](Grid&) { called = true; }, "invalid good");
                    } catch (const std::exception& error) {
                        failure = error.what();
                    }
                    EXPECT_EQ(failure, "Flow400 LinkProfile3 mutation scene baseline");
                    EXPECT_FALSE(called);
                    const LinkProfileMutationScene batch(good, model, profile, work);
                    failure.clear();
                    try {
                        batch.physical([](Grid&) {}, "unchanged negative");
                    } catch (const std::exception& error) {
                        failure = error.what();
                    }
                    EXPECT_EQ(failure, "Flow400 LinkProfile3 negative admitted");
                    OracleWork zero{0u};
                    failure.clear();
                    try {
                        LinkProfileMutationScene(good, model, profile, zero)
                            .physical([&](Grid&) { called = true; }, "empty ledger");
                    } catch (const std::exception& error) {
                        failure = error.what();
                    }
                    EXPECT_TRUE(flow400_resource(failure));
                    EXPECT_FALSE(called);
                    EXPECT_EQ(good.at(path.front()), "│");
                }
                flow400_linkprofile3_expect_physical_rejected(
                    good, model, profile, work,
                    [&](Grid& bad) {
                        if (source_point)
                            bad.put(path.front(), face < 2 ? (style == "solid"    ? "─"
                                                              : style == "dotted" ? "┄"
                                                                                  : "━")
                                                           : (style == "solid"    ? "│"
                                                              : style == "dotted" ? "┆"
                                                                                  : "┃"));
                        else
                            bad.put(source_border, std::string(source_plain));
                    },
                    "source face marker/arm removed");
                ++rejections;
                flow400_linkprofile3_expect_physical_rejected(
                    good, model, profile, work,
                    [&](Grid& bad) {
                        if (target_point)
                            bad.put(path.back(), face < 2 ? (style == "solid"    ? "─"
                                                             : style == "dotted" ? "┄"
                                                                                 : "━")
                                                          : (style == "solid"    ? "│"
                                                             : style == "dotted" ? "┆"
                                                                                 : "┃"));
                        else
                            bad.put(target_border, std::string(target_plain));
                    },
                    "target face marker/arm removed");
                ++rejections;
                flow400_linkprofile3_expect_physical_rejected(
                    good, model, profile, work, [](Grid& bad) { bad.put({39, 30}, "§"); },
                    "extra unclaimed ink");
                ++rejections;
            }
    EXPECT_EQ(goods, 36u);
    EXPECT_EQ(rejections, 108u);
}

// RFC 0031 T1-B: strict-first bounded flow router retry. Dense edge sets
// whose channels the strict disjoint-cell router cannot close now fall back
// to exactly one deterministic whole-plan retry that may cross earlier
// connector interiors (node/label cells, frame corners and port heads stay
// sealed). Inputs the strict pass can route are byte-identical; only a full
// strict failure consumes the relaxed result.
namespace {
std::string t1b_complete_graph(int nodes) {
    const auto name = [](int n) { return std::string(1, static_cast<char>('A' + n)); };
    std::string source = "flowchart TB\n";
    for (int i = 0; i < nodes; ++i)
        for (int j = i + 1; j < nodes; ++j)
            source += name(i) + " --> " + name(j) + "\n";
    return source;
}

std::string t1b_cross_group_mesh() {
    std::string source = "flowchart LR\n"
                         "subgraph G1\na1\na2\na3\nend\n"
                         "subgraph G2\nb1\nb2\nb3\nend\n";
    for (int i = 1; i <= 3; ++i)
        for (int j = 1; j <= 3; ++j)
            source += "a" + std::to_string(i) + " --> b" + std::to_string(j) + "\n";
    return source;
}

std::string t1b_labeled_mesh(int nodes) {
    std::string source = "flowchart LR\n";
    for (int i = 0; i < nodes; ++i)
        for (int j = 0; j < nodes; ++j)
            if (i != j)
                source += std::string(1, static_cast<char>('A' + i)) + " -->|" + std::to_string(i) +
                          "," + std::to_string(j) + "| " +
                          std::string(1, static_cast<char>('A' + j)) + "\n";
    return source;
}

// Draws at the 240 native width and renders deterministically across the
// 240 -> 40 -> 240 width cycle. Narrow-frame content is intentionally out of
// scope: a dense rescued graph near the 40-column boundary clips rather than
// flipping, which is unrelated to the strict-first router retry.
void t1b_check_rescued_drawing(std::string_view source) {
    auto element = loom::ui::render_markdown(fenced(source, "mermaid"));
    auto ordinary = loom::ui::render_markdown(fenced(source, "text"));
    element->ComputeRequirement();
    const int height = std::min(
        1040, std::max({32, element->requirement().min_y + 8,
                        static_cast<int>(std::count(source.begin(), source.end(), '\n')) + 12}));
    const auto wide = capture(element, 240, height);
    EXPECT_NE(wide, capture(ordinary, 240, height));
    (void)capture(element, 40, height);
    EXPECT_EQ(capture(element, 240, height), wide);
}
}  // namespace

TEST(MermaidFlowRouter, StrictFirstRelaxedRetryRescuesDenseTopology) {
    // K5: ten edges over five ranks seal every strict channel; the strict
    // whole-plan route fails and the one bounded crossing retry must close
    // it. Pre-T1-B these sources were whole-source fallback.
    SCOPED_TRACE("case K5");
    t1b_check_rescued_drawing(t1b_complete_graph(5));
    // K3,3 across two group frames: nine cross-frame edges must likewise be
    // completed by the relaxed pass without unsealing frame corners or nodes.
    SCOPED_TRACE("case K33");
    check_source_policy(t1b_cross_group_mesh(), true);
    // Labeled dense mesh: edge-caption reservations still hold while the
    // connector segments detour through the crossing pass.
    SCOPED_TRACE("case labeled4");
    check_source_policy(t1b_labeled_mesh(4), true);
}

TEST(MermaidFlowRouter, RelaxedRetryIsBoundedAndKeepsWholeSourceWhenUncloseable) {
    // K8 exhausts the strict pass against the fixed 1,000,000 Work budget; the
    // single retry cannot be funded, so the complete source is retained
    // exactly as before T1-B. No canvas growth, no partial art.
    check_source_policy(t1b_complete_graph(8));
}

}  // namespace
