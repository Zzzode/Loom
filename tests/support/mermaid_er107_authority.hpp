// Test-private ER107 B2 preproduction authority. No producer, file I/O, captures,
// report state or admission. JSON views borrow caller-owned immutable documents.
// Raw byte verification may precede B3 captures. bind_case/deep validation MUST
// follow that consumer's own frames; no preflight Assets constructor is exposed.
#pragma once
#include "mermaid_er107_source.hpp"
#include "mermaid_relations_oracle.hpp"

namespace loom::test::mermaid_er107::authority {
namespace source=loom::test::mermaid_er107;
namespace relation=loom::test::mermaid_relations;
namespace sd=source::detail;
using source::JsonVal;
using source::OracleWork;
inline constexpr std::string_view kProjectionPath="mermaid_native/er107_terminal_projection_proposal.v1.json";
inline constexpr std::string_view kPresentationPath="mermaid_native/er107_presentation_source_audit.v1.json";
inline constexpr std::string_view kProjectionSha="f8f7dedc651112ac553ea86d200f2562ffb05d54b2b09df7d6477a687a8de7c9";
inline constexpr std::string_view kPresentationSha="8f35938eb74591a428823aae3c1279c4a333fa8240b7d92181e77e7f22ed18ba";
inline constexpr std::string_view kContractSnapshotSha="eb6874b488d076a481fc184b7819ede9112552dbe11e2b5719eb03e55c39e8c1";
inline constexpr std::string_view kReviewStatus="UNREVIEWED";
inline constexpr int kDrawingCredit=0;
inline constexpr std::string_view kLimitations=
    "Preproduction source/typed/style/notice authority only, not public capture or Work proof. "
    "Shared successful selected-trace contacts preserve complete host style; only owned U+0020 side clearance uses root BG. "
    "Multi-edge contact ownership is supplied by the shared qualified oracle; all107 complete-scene feasibility remains unproved. "
    "Primitive geometry and full style cells are checked, but caller viewport extraction, complete-scene route/label ownership, "
    "cache/fresh-node behavior, general Unicode and browser predicates are not certified.";
struct Pin {
    std::string_view path;int global;
    std::string_view case_sha,model_sha,scene_sha,binding_sha,complete_sha;
};
inline constexpr std::array<Pin,107> kPins{{
    {"er-diagram/pr-check/should-render-entities-that-have-no-relationships-usemaxwidth-false.mmd", 485, "45c92f1959550af047955c567d073dc67fc8bcb3bf38f0acb9c9319d246773e7", "6bef4c0e5d113c4b2af80dc79c1158b6c9d5ecb51ffa91bb959b41f9fefedba0", "008bf49980231b8cb8bf67edd9359a28020d73d05e6ad58172c3ceaa0d3d722a", "03e43da961cf15c789f4fab140f0550fe66fa60b210d3706b4e519d7faac121c", "d0f5cbf0a074499e20d1b511b46410f911a0834330d85b52536d92e383fe1572"},
    {"er-diagram/visual-check/a-cyclical-er-diagram-handdrawn-look.mmd", 490, "ce4f4b65384082fb5ab7484dd4e150f2bbbaaf2ec39720fbef76733de47b511e", "da220bdb27ca3250b97c5e258444eb362f13205fd5d61a4cb26101fc0e5bdf63", "6d2815dd41dc88c02ca893666a6223d834bca58558f396d75c8aed0ed4c64846", "f801fc990277c12c562297e75e8d5c1cbc0e34aecb35259be51c0646eceef83e", "b38bc9d4d0948a5ae935d6e1cffdd0a0d1a800b9fd8696085399c34238ba0016"},
    {"er-diagram/visual-check/a-not-so-simple-er-diagram-handdrawn-look.mmd", 492, "e4659af4dd022c7e57d5c5df2d8b2beffc8bb02596d18b34cb5123b8267567c8", "6c50d40db928d4ca4d2d17410abb1b047ff22033eaaad38ed0308fa4b47f93d4", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "b33010f533e772697823628eeaf9e30aaab5b43a45692381474fed98249f3fd2", "c51115654a6e27bbf26ed4994d08b65d9321474521dac7b8272e36d7faad4639"},
    {"er-diagram/visual-check/a-not-so-simple-er-diagram-without-htmllabels-default.mmd", 493, "b02e7504474667ac6e78f1539e9e04bf14168840388a5b2faf5e026fc3a0d8aa", "2f665d6350ea30c0606a43824e88c8b6af09449e7e7b8d9d9d54c6556a98adac", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "1bc7560d79c2d691babc14446cedde73b04e2f66f298589f401ba6364a8e2987", "db1bf3a7e3b2d0e179d80e2eb0ad0523ef3fa9ceccc745aac38e494ae3d598dc"},
    {"er-diagram/visual-check/a-not-so-simple-er-diagram-without-htmllabels-elk-layout.mmd", 494, "235ff5e122452fd1abebf99ee212aa51d09e110c1f8886b44450341c69582c0f", "43251197a9ca24f235d6e6e2c21618d82eb84b97adee4859e199482eed9efef0", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "20b61799331a794f4338cbae8ff76a5bc877a70232368bd2e184abb547dbe716", "3a57d59bf7af4d651509ab61e766d98502700cae9a6ad6338626f043a3e59b2f"},
    {"er-diagram/visual-check/a-not-so-simple-er-diagram-without-htmllabels-handdrawn-look.mmd", 495, "fcd4b242f8328fa31a2251b1ffa679a9cb9b02fe032a82ed4e217465b3c384d8", "5bbacca1b0fa7e3357235c8ec93647b46fbb081f0c1caf867d558342e26b4cae", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "746fe6c51794d2e3d3b12b9ce91c83f068e94993517eff165d3ef03d9fa00e57", "1aaf392d3c014513afc19f866cfe4c7d44298d128b8b8cf8662173d42329d525"},
    {"er-diagram/visual-check/a-simple-er-diagram-with-basic-relationships-neo-look-neo-dark-theme.mmd", 496, "5c2f685fcbaac2ba2f181749e55a2fd6c1c5b2d3fbd9fd26a13197e0f7f2203c", "83ee2c35e816549d8d8f2e359486de48b7e553c162fbca1b85e1df8bfe4e6075", "1a64d3c2fa56b7a210167794870ad45d4637e7610f6d2132d17707cae2bd9e5f", "522c51cedfd290a8fb967d169b39da91b439c415e5bf19572efa0ce3df62967d", "31c0205fc02027025597d5df286fe6e0533a9150de7faa78b03bbaf79306139d"},
    {"er-diagram/visual-check/a-simple-er-diagram-with-basic-relationships-neo-look-neo-theme.mmd", 497, "957c24e779794d5d17040b599626173ba914ca13d5e9a8189984d60af49d2ead", "1be92c856da33b9ff00812094600bf8e2212a7587d45191dba766377f8d40ec2", "1a64d3c2fa56b7a210167794870ad45d4637e7610f6d2132d17707cae2bd9e5f", "436d0684df2f31141fe049e87343a3429ddd238fe923ebfc7dadc600af355a5b", "b4cde31bd8309b205e4a8c5e36075d718f9f1e8f5c7d46dc0e4f51ea20450c5a"},
    {"er-diagram/visual-check/a-simple-er-diagram-with-basic-relationships-neo-look-redux-color-theme.mmd", 498, "2df8a9c7a7b6504a6fd4764a8191f1913e1da2021e13d88b4f9a8ddbd7d8a38c", "05400e6e6bf4f75027a95d14d7bbb5fe64b1501d46f8e0093dbeac74c48b23a0", "1a64d3c2fa56b7a210167794870ad45d4637e7610f6d2132d17707cae2bd9e5f", "e3c840c6cfd962a65719316b720da367b9c4ae8328690b79b1727584d6e8a33e", "622d4e350e55514d49043c7a0b05750e0ba0fafbe3890738220dc657521669aa"},
    {"er-diagram/visual-check/a-simple-er-diagram-with-basic-relationships-neo-look-redux-dark-color-theme.mmd", 499, "44c71751d30f1821d32cd06899d63be3877334b792317028664ab245d265bef1", "9af242f47bee4d4fa8153ae88fa867af92f58342c840f34dcc3ec2ec3e6f79d4", "1a64d3c2fa56b7a210167794870ad45d4637e7610f6d2132d17707cae2bd9e5f", "9ed1b52a4ab7891db52c98e9f79ad5ba5aab7f579822c2e12228d9afc7699a5a", "eb4303d0bbee870fa498dedb6dd198b337073359818ba1404b8196f9ab7bd70e"},
    {"er-diagram/visual-check/a-simple-er-diagram-with-basic-relationships-neo-look-redux-dark-theme.mmd", 500, "9749e44a1d2781bf972a6d51b20c0712a5ef07d9cc71ac336012e2519b2bac0e", "427e260243324d1f246e62a84d9d16cb26a642e7595e38310e7823de09da2799", "1a64d3c2fa56b7a210167794870ad45d4637e7610f6d2132d17707cae2bd9e5f", "5895887bff47a7c15126acbb93c8d59df8cda78efae06087b3a5bddf3eaf3ad8", "49df9f4fd902d7fc543f9413a649d9331243fba95a7ba71f1f8aef016674a409"},
    {"er-diagram/visual-check/a-simple-er-diagram-with-basic-relationships-neo-look-redux-theme.mmd", 501, "788190ba68de2802a1ffb1a933a38e55fd72fa62e1154fdb11ca101b52cbafff", "acb7b62493bbe6278483b6baa444877ed3b1ea8b4f71d65f2c7c9875d4cadafc", "1a64d3c2fa56b7a210167794870ad45d4637e7610f6d2132d17707cae2bd9e5f", "f719a55d468caab27b26963a29a327fe54177441909c43b3f9004a9dae12d366", "c5df8c8c9e298606d23d7302d10fcb7c51276103130269252fd20c23fdcaf751"},
    {"er-diagram/visual-check/an-er-diagram-with-a-recursive-relationship-handdrawn-look.mmd", 503, "8e250244067bd70d37ba149cb4348e8245e4097ecd932e7c41654e51384a5d35", "5ec988dc38e603f375db5e7de4704750d125881d87fd004d42d4a658fa555027", "d4771fc94dc26a58afc2a3d51e675b7bf35b2c261656e9204480602c1c1ac5bc", "d38d7c9c7286a674d915bb3861a9dac103fd97c3b1b0b5ab623397f444782334", "f0b90149effb89fdb1ff5fba2cd649be2782338c05210adaadeaaf99e739e6fe"},
    {"er-diagram/visual-check/an-er-diagram-with-blank-or-empty-labels-handdrawn-look.mmd", 505, "2de7aeafc3d22f2b34f946d2fc77b4f852465d6d1ae9bba97bff77e71af44012", "576a334f7bbf365e63ad0aa32cd536ee84fe338dbb2f0003b3586eba0e001e85", "ddb718a02097327822a45bcc2bfbb13d95ee97efc05d19df00003b08d16d8eab", "1c0711c679a2bf920d7c9df2941d771c777a5de85d31c9b7e4660c8c11f0f6fc", "3cbc5347ba6acece06c7adeebc5fee50e31ce88a9b2ebed60e46251e2a98acb8"},
    {"er-diagram/visual-check/an-er-diagram-with-bt-direction-handdrawn-look.mmd", 508, "d08f0cfe8584a3f2c0d373a645899a180f180f42b9db4fd2172cc636de0352a6", "8e5e82ae8830333b70f9163cee7a5934027437ee52de54407dbce54f513bd306", "9372c49f5f77fc40b8456bff647063f7e44c2e36c45d12548651135f6c1a7e41", "03e0353ea7ae63427bb9212afb88b964c744298aa26afd3a03efbe485363ab9e", "dd75aa90eed7274acd39eac5a3e5b3814c321884b2f46c72e995da8968a3b65d"},
    {"er-diagram/visual-check/an-er-diagram-with-lr-direction-handdrawn-look.mmd", 511, "bf8852524410cf286770d677a6d5b6e1a487e2054ce428f532207d0e827131cf", "93d34ac1f8769bbee97b984720ac7e13a336d396b37d131facd791fd901643c9", "5ddc6cc592ac9ab3add5c8cbcc30e95d7abf6a21ce5566f5a734360021eb7f06", "7e93c2cd18190795bea44df94e8cdf6535769ea1bd8730b8dcba2a0697308909", "f277fd340075d099c0fda4526e1125c1e6b71f2e6ae1c7731c61c7d35fdf0b6c"},
    {"er-diagram/visual-check/an-er-diagram-with-multiple-relationships-between-the-same-two-entities-handdrawn-look.mmd", 513, "05f2e63afd1187acfad0ad8014e5416eedaa3f70b9f31a20b78c406828b81f4b", "847df460d1ca37a48804f9894cec64ff4da61d78d715cc12c456d9003314b3cd", "c0a3361de3893f7f342762cf4d4c5ee31ac0e38680cdf46cf7195ea7cb799250", "f9a0787b08a6ae1e36dbb4aa03b3fdd8713f4da15564f755b62a434979efc29c", "63c818e68dd0492dd1d55c1205fc8ad078d10b994aae884d1586b91de3b47de4"},
    {"er-diagram/visual-check/an-er-diagram-with-rl-direction-handdrawn-look.mmd", 522, "a32ec5fec471bbd85b96b58d668598d31d3aff7deb8322fb47158ee0b71c8f1e", "e1b5d839e9c7f0f43316faa21f8ebe2555122b7e2776bc7133d22ab59f4c550c", "4d5f47d5b1336483a62269352732ed567bcbed4440bc80007bfc4b7520fcbe0e", "4ac2aff975cb0fdf99bd72beb29938aa9322a55ab962df6cfb7fbebfdfd88518", "b97cca447a1166b85c4009fdabbd410b7c3a96abf9438896c6e7211c52cc8d25"},
    {"er-diagram/visual-check/an-er-diagram-with-tb-direction-handdrawn-look.mmd", 525, "c0f1f540a7d5283b4113bf8ccbb70bf48357a1f24e958e38966490b98fce5a60", "2a0c476d24d0f5914b38c83da123b6e9e986936cde8fa2351545d35b21159ddc", "10f1c99e3572fa4f31f1d628e55087dc0028cacf1d12438097c37dd9a42dee03", "544c78b8e4abdfde5255874c9b1dacbd35b99204f61ba46f9e05dc7298abce50", "8f74ee813027b4659f79d69ac2addf92e34b5cb1d6143bcd20596856769c6fa8"},
    {"er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-neo-dark-theme.mmd", 532, "d10c67d4ee4150debc72a63eb290492fedd9d38ea8f6b4decbc6735f1d923744", "d2b74a24ac7504a4fdef2ee19c48528102a917ba31dad989909ce86c5b3c869b", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "4f02106a50dd2802f481d329158093b134721e4f726dea55ab55afd29808bedd", "c469dd89936078b44dcdfe0d159a2097980193ab953bb7c5b725244f421fe2bd"},
    {"er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-neo-theme.mmd", 533, "d56f1fa0f231b86f1c82e4ef8c7d76eb65b1e3e6dc6fe938dd1df42b590998d3", "21f4b1c34e3623364c4d1fdaea6569a10fb2ddee81cc019b01f90265e9aad9e4", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "0eeff1c6604e65ed166e3f08d211e90f2f7fc72d4c708f3b90a4970b9670e7f9", "0302233c519a62fcd23eadf86d69dabae64c54cbe52f9253d2ad9031ee8db681"},
    {"er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-redux-color-theme.mmd", 534, "ff5c74e910b952b6c8bdaedad97c2fc41e52f361b488e8f864b8cb5f3b436746", "e88d0614d538bbc45c4829a4ec2d7a7a0fcc952315ed01bd4067ea74032464f2", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "dd5c36060dd0fd23e791c1bfac62928794f8438bb9ff5a42cd68adcf402defaa", "f9e8a5f66532fac9b08040316231fdb0278752fcbf627e09d7698f665f582c57"},
    {"er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-redux-dark-color-theme.mmd", 535, "cdddc3953267a908bbae220154e26d93132c36ce2f1e72cc76ab459a79392a2f", "eabbe092eba74b6f67633d2de3484a8c029a1181fc0fd3928c526cd0fae85186", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "d19be7857f69185bba54bb076800e8da12b511a33468582ca177d3450fb994f4", "0c3d460954451e4f2408a20d4e8dcec160f07907088cca6ec778a26b1556f054"},
    {"er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-redux-dark-theme.mmd", 536, "9dac46edd0e6d957dddfb046cb9ae23c2a78a8f6a2b12a7e74a3ef977511f2d9", "6fb6a774a628305ba0234ee844753b2868517170320eec8d7c40b6a77088f786", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "184c2f8d1d707f3be5d1cdab002a3a84faf1a415c948a74b259050ce909b1b6b", "96f014b5784f9e1f756502a97b3ba5d49cbd54003f1f67a22006b9cb80a3fa3a"},
    {"er-diagram/visual-check/complex-er-diagram-with-multiple-relationships-neo-look-redux-theme.mmd", 537, "81293ee6c36ab3893b1778b0898bd4660d9910deda86f488f4ca65f0cc14ab3c", "0fc03d911bc7fd0badc23743335b9a0de0e80b35b10db4d3acc5b895dfbdef06", "c80b3f4d6b4024b531d690f38538fbab348b4fc928390a7fb615d658f8375227", "d60e180ee469a1e7251787aecc44bc8a2c40fdd442ee1dc9b45d462ac3d5cf06", "107730e094717489aaddc92c9d8740729add71663006f73d995a236b51ddf96e"},
    {"er-diagram/visual-check/entities-and-attributes-with-big-and-small-entity-names-handdrawn-look.mmd", 539, "d2340a1daf0b43b943181fe2515e35eb97875bef38fb739d2e9e288c40655240", "18b0eb3e9d7b677e9c9cf2719ea461c1461b6ca4a0472d96c2f76d3bc03e572a", "f6b5296b3a9e4d3ce403bc907797c2f067a8e2d9133566e321ca50704007d1b7", "18f13ff6156054d6fafc74302adc45ed5bba39ad2376b7d501dc5dcb9f89d56e", "0272a5c6b438a624fc0efc2d27b0d627fd6414824987aae6f0a403c1544cca04"},
    {"er-diagram/visual-check/entities-and-attributes-with-big-and-small-entity-names-without-htmllabels-default.mmd", 540, "dbe15cf2eec077512b9be7574f18982721473d1edc0010a876c1a9102302209f", "ba0595a8991fa34985ab3d0aa278c521f617e4124a7674dc62ef2497cf4f4bf1", "f6b5296b3a9e4d3ce403bc907797c2f067a8e2d9133566e321ca50704007d1b7", "04d81ce5d6bf3df61f861041c39746a264f5c6a73653d880ac0ce7b1256a9c2b", "66dafbcbc90233c85c280c81a169afb1dc49acf737fddd3bbfc6bcb402389d36"},
    {"er-diagram/visual-check/entities-and-attributes-with-big-and-small-entity-names-without-htmllabels-elk-layout.mmd", 541, "3d3ac34cb929a3740dfbe017dae7c1ec522c858814dfe7a5e4634b887ca26a08", "3751b7fda029cad2b0ee4affaa232bd24e0dd88f1940cc3620b173dcd4fdaac3", "f6b5296b3a9e4d3ce403bc907797c2f067a8e2d9133566e321ca50704007d1b7", "f4a179aca3d0272441f34ee198e792229c42e9cfe07a12b7e8da650b16fad5bd", "9cd2ea2a8a19b464b19e9e954400279944b78e32ea3837d2e377173df460ca81"},
    {"er-diagram/visual-check/entities-and-attributes-with-big-and-small-entity-names-without-htmllabels-handdrawn-look.mmd", 542, "19c501c626f6e970b6b8fb80de41098ca308efd48f365ac25136550b19c4c276", "399b84a8ce598766d802db95636a242b4b0e5d1f3997c68c822868a43dc5f206", "f6b5296b3a9e4d3ce403bc907797c2f067a8e2d9133566e321ca50704007d1b7", "b31ad333d14cd901ce6e14a72eb9cd0a22a19fdd2d6769f51d5bd0580c024921", "d95a82de051c5e79b2307535f5e41152d00e9cd2f79908398e2b24dd049a883c"},
    {"er-diagram/visual-check/entities-that-have-no-relationships-handdrawn-look.mmd", 545, "00ddd62fa6765c7a56c454e08e4167c023700a6181e38a712aa8750f6d2dc8c7", "fb981a73002f73c7e04fced082340946369c1f6c50ac5e5bad81b8ad70ae3bb6", "008bf49980231b8cb8bf67edd9359a28020d73d05e6ad58172c3ceaa0d3d722a", "8aa5b2dbfec95e9c75876eb0fd2c1067af615f57069f4b85fce745124ce6e4b9", "34c48fe50a61e1978b99c55a4cbf6d92ce9e868f75674f2c222acaae6cfd695c"},
    {"er-diagram/visual-check/entities-with-aliases-handdrawn-look.mmd", 547, "a1d32450a01da4dba16b54348cd21e9baaf09c5f1be0ccccce72b4b28f24119e", "f03e6908c887eb8f46e1e4f06896429262fba14a2d8e1987f931bb4a5f3e901d", "7c8e2d2975c52e9a274ac484212ea2b6a6c147ce4858c0b22234d5fa09b52d3c", "b025d5f2d208ad075718d2cda586430e280505361837393f31838d9b70474c22", "007b168ed646917a516d1aaea2a4c06d475ea90d4e8035e9bbcbdd2a4d1d9708"},
    {"er-diagram/visual-check/entities-with-and-without-attributes-handdrawn-look.mmd", 549, "b7d0a70075912326981f1fc3401f56b3737d95c66ae57a12866376d246c696fa", "a519220e1c389c677df907f5121c9fa383136ffd6929917d4d38149b23178a50", "691712af52ccab2696386da6f8bd0700e2ba6857f29e55d1495c6d1d2a17f74e", "0bbc1b6c94a6cf9ddc051b222c375f2779cc01ad5ab2544c4ba219d2becf815a", "0420cdd2de205c50e7ab55f547ad9324b922902d628decd8fd83c66c5551be60"},
    {"er-diagram/visual-check/entities-with-attributes-that-begin-with-asterisk-handdrawn-look.mmd", 551, "58a244e6a7c4c9c6ca79cb49f5405e46d80d11870b82c51c05555174dd7d7b16", "34f3e7cdeb42876ad7af71570c269f7f1ff17d29af1d4348c746ef9b5072d902", "94cba4d5014856ef607d837afc8b3f14a3e22568af179cbe3a6004e35783a14d", "f15ea16bb08e53aecac3806e29e2a1d23c116be93b44bda90ad0b017447c376e", "8efe51e7ba4ebd850bf7ecf0fffd4bdeccd08fd0d687888818b440c0ebd246e6"},
    {"er-diagram/visual-check/entities-with-attributes-that-begin-with-asterisk-without-htmllabels-default.mmd", 552, "c1964902bbe03a444f2e9ed16b5510670c6ef572bb416fc50fa697ece6774e69", "0c73c816d6cae74491af72b29bef5556e0dde3ae1de2e053e99c7fe36ed4d032", "94cba4d5014856ef607d837afc8b3f14a3e22568af179cbe3a6004e35783a14d", "ea82e52d0baa71984cef61b3f59033d9e8a1d3059dab6871e56daef31a9bf534", "13feeee7f1d59af4f5eaefb2438e51e9186feb56d806ebd53fc69292b162b2e6"},
    {"er-diagram/visual-check/entities-with-attributes-that-begin-with-asterisk-without-htmllabels-elk-layout.mmd", 553, "3ef556425828abab4a79e12253ed54d5fbb50230a2ee954016f4fd291184da74", "e9f872500d66fed02b13ad0f3c37729c7474b4fcd1e0cc2361d0e88836e56fe5", "94cba4d5014856ef607d837afc8b3f14a3e22568af179cbe3a6004e35783a14d", "52449cfb221603ffeabce141b2c11da265d49ba9197cb70b452589d48feca054", "d0358917dba0ff6ef090bb04b02f0f94d942e0bdf2d8fad39f4dd8eb6a544366"},
    {"er-diagram/visual-check/entities-with-attributes-that-begin-with-asterisk-without-htmllabels-handdrawn-look.mmd", 554, "ddbcbfc62ce14e9963aaad80f694378a75831e269ad8d6785a47e46ba868fed7", "c95b7e72309ac0872c22338c22802d42d64486dc266284795b344e8752cfec2c", "94cba4d5014856ef607d837afc8b3f14a3e22568af179cbe3a6004e35783a14d", "1648f9c3693add78d6f4801eb065964d8c0ca1f627721f5cae8e27a5b4491a64", "ce140a56b942baee59f6fe6e7cde87fe90ce7186b119a9610bc9831083dfb23a"},
    {"er-diagram/visual-check/entities-with-comments-handdrawn-look.mmd", 556, "90ac2166be4aa4daa536c5a17faabd02edb6a6b29b3af4186fcbe2d20caa3f0a", "632df6d39a11659db51611bb8de080e418cef087a21c929a0a70c4be9f3b583b", "cd5cb6971a3cd0ee23768520c220d8240d57459ede21ca4459ab451440af7348", "5b7aa6e267252e4585419e0c0fc29c01b76f99189b8e14469cb08ddb8d7378b3", "0385b7e6e6f024d7fc50e6808b48c4bdee6bfed0e41f321be232d7fa80b697f4"},
    {"er-diagram/visual-check/entities-with-comments-without-htmllabels-default.mmd", 557, "91dd6ec780071d83751bb1c4eeed6fe16c407dfa0fef45b1316f0021869d41af", "07b03f94d584542b49feacf16efa5d7d70d52999de37fab475925fdd808b8b9d", "cd5cb6971a3cd0ee23768520c220d8240d57459ede21ca4459ab451440af7348", "24c0398ffb6f19f7e780032ea47db822d904795ef63835c000d37d5338d8ed5b", "f5a470eaa01344d517a7c7380055749b3687011bc8d4ff9c29adc4fc7add8e53"},
    {"er-diagram/visual-check/entities-with-comments-without-htmllabels-elk-layout.mmd", 558, "9ed95b3cff515d9eda9e57ed0325aacc3ce7e4dc97e1ddd03c8f88cb76cb8081", "d00e015456b602f209c116a560d2c80171a504d1a98dd1c205a9231259a0a657", "cd5cb6971a3cd0ee23768520c220d8240d57459ede21ca4459ab451440af7348", "9f4f5f58b9c11ff8a8f5a1ba869afd3922de0262d9f3e38056b9400488a6a8a3", "96b7bc8d26ed7fdf72b144205a03a0525d76fcb57f437fac30a7e67efec023a3"},
    {"er-diagram/visual-check/entities-with-comments-without-htmllabels-handdrawn-look.mmd", 559, "5aab9475c88c8c53ab9f2c0546959d6362d24dde81629acf8fb7a7e2e29a9ef3", "132f4d73a41f536a0ed329f61f2025b1f7a21622ce5528c4767dd01d0a89c20a", "cd5cb6971a3cd0ee23768520c220d8240d57459ede21ca4459ab451440af7348", "6262c343c102317d5e14358f3992cfcdd4490f9485aecd35328e1749bf9f0076", "72fd81de033a026cd09cc04a10bb6239fc406bfd69f72587af36fba54994abbe"},
    {"er-diagram/visual-check/entities-with-entity-name-aliases-handdrawn-look.mmd", 561, "6de9fee2e2c2ec76b61c75182fbeed39b0a1f596a62c0783477b8b97a191f2c9", "cb9f3dd7c0f92df59d474b4e8df92b3e7f977e2d7c045e2c53d789050731a88b", "357481402e5881d9a588a643ded6c2b035ce0f7058c5459167b673933bc806f8", "a00673cc31f954fee815d7b2c2ed8a312409b3ffa936d39f9d3fd2a7afa2d035", "8b1e4cf56f27ca7b516bc81c68d15aca1d509435625aea45b36f2681c8e219db"},
    {"er-diagram/visual-check/entities-with-generic-and-array-attributes-handdrawn-look.mmd", 563, "d2e4672f5fa1dced809c5009be2ee95eb2ae507236c67f89e2cad2bae6b15bdd", "eaa194f8fd8a90077e1f47484069ae36e619ad94c093ebf5dc4a3f7767cac251", "9c5eddbf0081573c0ef55e1c1cb5215403a6f52557277be14e67a5562ba36c96", "a1b7f9abf7585d43d997159a41723864bb4eb91f188b096455f815d154aefbef", "f24b000d14fabdce00895e2d9bec559b9e3b988399b4e6453cc5e8d0f99eb740"},
    {"er-diagram/visual-check/entities-with-generic-and-array-attributes-without-htmllabels-default.mmd", 564, "733646abd8b4edc40a6cd1a1f85c190b98ba557cbe0dea24551727ab80a54004", "2f93916e2a150deb25eb10fae4c32a26e59022189274136c32b6436547d0819a", "9c5eddbf0081573c0ef55e1c1cb5215403a6f52557277be14e67a5562ba36c96", "5c32777a05b1460f78ede622b3fe74dda732eb08333deeacfddfa4956edecc74", "5585286bd90f2ceb4a1e9fe07f00f139976d7e178637870f02b98d32a785e572"},
    {"er-diagram/visual-check/entities-with-generic-and-array-attributes-without-htmllabels-elk-layout.mmd", 565, "79a0f4e7167c12713319a36b45062d793feef9b01004063e7fba967fcedf1f3d", "59374237319df9f92110833504c957a7ba5f919210f07e4b635ca0b87f3f6a9f", "9c5eddbf0081573c0ef55e1c1cb5215403a6f52557277be14e67a5562ba36c96", "8dfc8f787c9703fe83f7cc62b85683727f15918296bfebd9209826a1a4040ec8", "49f2dab04327c6e2a4847a5d5e64cfb1f403394980efde5902953d6515c3d96b"},
    {"er-diagram/visual-check/entities-with-generic-and-array-attributes-without-htmllabels-handdrawn-look.mmd", 566, "1b2560bb384c32e1850d4c0a822d16e472c246ff7466dfc3998c00e019b40976", "4b8cf56f9591271c1c312a6d07cee626569c6dd215867f21ca84eb052cac359c", "9c5eddbf0081573c0ef55e1c1cb5215403a6f52557277be14e67a5562ba36c96", "26f34a70c6a7ae2d68bb739aed203002057d75b9ad8ddcfa5c6e97052c65635a", "e52f2e729022436df0769ca2e7be1741d5a617209b9686547ea532605be7c1da"},
    {"er-diagram/visual-check/entities-with-keys-and-comments-handdrawn-look.mmd", 568, "19bea1e3ba3b0c0e8fcd0b9e3673b556c0b5873c5608c0e3838484e6c9c3025e", "91f8860b944362eca0db1cde8acdccb315efdbb71addd5c0c0f759121b96c202", "2007d5108bdf020d3993973a06649e04990e7e39664e8f4d8704f8af260419d2", "2880a389756194409da3a05dc7c388bd7acba0167840602fe9152bbd12ff6909", "4c3399f47a0c788ce165a748ff97b6f301b1d626ba42fc6d9ea10fc60e261f93"},
    {"er-diagram/visual-check/entities-with-keys-and-comments-without-htmllabels-default.mmd", 569, "2b7ebb9b455f593dc39b11f32e4aa7c9f1a3f6ca7c84b0cb92053a383b7f04ce", "6590ded66ed11f3b1785603c71791d73e2a1a34c118020f9a8ebd06ae47514ab", "2007d5108bdf020d3993973a06649e04990e7e39664e8f4d8704f8af260419d2", "0ff976bc7cbc1fca7c05d56a144ed25110c36c60c45999283eeda49d4b90a871", "d033b5514a777c70d66795e6a2ea791d99c38cd5b0b660166210d6c7946d4ed4"},
    {"er-diagram/visual-check/entities-with-keys-and-comments-without-htmllabels-elk-layout.mmd", 570, "733f36261ec090b8f01fe1351ff4fe896a56b1b237c248efb94c96ac959525f3", "201bb7044abfbb8cd9da976bddb893b000c688429bba2ceeb06d7e8842a853ba", "2007d5108bdf020d3993973a06649e04990e7e39664e8f4d8704f8af260419d2", "8f8d7a5f465cab6e4340daecb8d47c63e291bf41164c51051f1a0d47596e42fa", "bd315aff2650b5af7d846af5130cd72ced62eaf362e9ae3209f82a921ce31472"},
    {"er-diagram/visual-check/entities-with-keys-and-comments-without-htmllabels-handdrawn-look.mmd", 571, "a80b2080c9fe2e02f152fb6d902cd0e77c5c3912d9fdb90e09b7d30ca998862d", "f4d941c9894d21d8279e6ded06a67013e4025d8385f725cc3f763798a9c4bc79", "2007d5108bdf020d3993973a06649e04990e7e39664e8f4d8704f8af260419d2", "e8c02e3aeffd000f47c68445ef4a02f4cb4da267fe5b7f69d3651914af2b1cbf", "a163605784ee16f87a9c96630842e83015898533150553326257e7b61b8521a9"},
    {"er-diagram/visual-check/entities-with-keys-handdrawn-look.mmd", 573, "3a0fcec7ab52f07257e61e82ee0ac8c20bbb904f4cb9fd0177308530a45827bc", "1011645b85d8a6e34b3ec38c213ddf3060f0211170c70d49417d66271627741d", "b91a37be305aeba6e9f0b3e07c89ebee4eb236e5433e32be475f65221f76b63c", "ed2220d1a15aa551a86800981305b37f161367535fe89125b7a5f11cb787f85c", "4c8409d7a2033ff8ae88b1f45ed6c276616238be7b7daba7346be224aab513ea"},
    {"er-diagram/visual-check/entities-with-keys-without-htmllabels-default.mmd", 574, "c6a2d1a848afbc890625d739114faf6174d527d6194b3f4e56edf2090df8784b", "07c93760c11ed3fcdd62bdd033fe78045e29422c8fc34c4d87e7980e3f71d053", "b91a37be305aeba6e9f0b3e07c89ebee4eb236e5433e32be475f65221f76b63c", "b82863bec350cbf57549b359b953ab5f7127ce8abb315b980da9319c45ec60e0", "80ca1be9ce0c7fe7a7ec1942597977e3cf569df506452b536935f9a46902e022"},
    {"er-diagram/visual-check/entities-with-keys-without-htmllabels-elk-layout.mmd", 575, "828a15595e1aafe4b0dbdadfe232a4fea4bd9c50a2d1200c85e9356109fa6573", "bdd33796389a1f34d1f20f43a9fd2b035696326e9dc615ab39de6954a5563d67", "b91a37be305aeba6e9f0b3e07c89ebee4eb236e5433e32be475f65221f76b63c", "8269d2a83147a6255034bd8624d5cce4deedcdd832537e1c5002f9c25cc4dcc6", "350c5dbcd97578a4af722dab66b67e97d288deba96c4d067a7546a925e83a66d"},
    {"er-diagram/visual-check/entities-with-keys-without-htmllabels-handdrawn-look.mmd", 576, "30c528764b0e117ff5a49a3ee5189263625af77cc9d2a37f5c61a7ae885a193a", "e8b16e0d0e886e1e74fb0665afe5712920b175224ed0d4413264c4ddea620f00", "b91a37be305aeba6e9f0b3e07c89ebee4eb236e5433e32be475f65221f76b63c", "679da85ba8addb4367ecbd1f9066753f71c86345785a82864a9133089511f030", "4cd43c31624094ffb2668d315ef801bdee0d809bddfbb4d907d2571cf3fdd501"},
    {"er-diagram/visual-check/entities-with-length-in-attributes-type-handdrawn-look.mmd", 578, "90b1bff8ef15f5b800a5ac17a5505cab2ef8bef1e2ba3bb1df9f6f07f37db6a4", "e3a5145283830288b1fa203b3e1e061be7356d213a5c6dba43981e7bae4b4854", "d473e00f584aff4b6b2805d3a958f779d969fd2ab7f1587ea7c1095dbf4387ae", "1e5d71084063862eccc6b1faa40e7e7325268fc2c57a7c1051586503728c7ed6", "534b85afd10895d954eb864af1d61679a9afe9c8fe316ccf625109571d69317c"},
    {"er-diagram/visual-check/entities-with-length-in-attributes-type-without-htmllabels-default.mmd", 579, "af30ca046615fd2f9e6502e339592450f3cbbe14115d3d1a68247c622916fb1a", "d32756b2b9f011f1edb06079dbb38652432d220e2b110f9c1359baab061ce0b2", "d473e00f584aff4b6b2805d3a958f779d969fd2ab7f1587ea7c1095dbf4387ae", "e3dff3f641f38c8e307fa562ade4e96e0a0c4e4547b33d1da5da4adbdb12f7e1", "1865c6215926dba93fa0ba800fd48a66dfe61dfc777e6e7809bf069a47d1b8f1"},
    {"er-diagram/visual-check/entities-with-length-in-attributes-type-without-htmllabels-elk-layout.mmd", 580, "a5448b2858f81110ea1fcc557c6403dbc2017b0160061cca2cf258d95d6d3f26", "43463624999e3594d65bd100ea504726ecbe183a7eda072cc098f3546211a472", "d473e00f584aff4b6b2805d3a958f779d969fd2ab7f1587ea7c1095dbf4387ae", "5f2bc9a7a9b0e754d9731be190e9b14d95b10757c1725e48fd9895da6017cd07", "8edaf6e25acb24fa0c9acfb1038f7871c7212a6f35fd69a99d3c2c53459a2154"},
    {"er-diagram/visual-check/entities-with-length-in-attributes-type-without-htmllabels-handdrawn-look.mmd", 581, "3e693de4b178b2c918a4e920a026a6aa5fc028718c8dea0ff4bed630756345d1", "a90b79cfa531f9833f5fd94653839280e6c7978041e7fe3ecf289d0725d8259a", "d473e00f584aff4b6b2805d3a958f779d969fd2ab7f1587ea7c1095dbf4387ae", "73eec28c0312a124f8d41945c2f84290dd5e7865672fe2a786a28e33e012272f", "22c325521f8ba08c8e5769eb6be44042897f054d39804093f4633045629c41a8"},
    {"er-diagram/visual-check/er-diagram-with-all-relationship-types-neo-look-neo-dark-theme.mmd", 597, "e9c064eaf191029b3be2ecd8240029c83954fe816b29b9025c5b459351498c8b", "94783cbc73ecdf4a18c8efe6a5750bdf2352965156a558ed8afd6312c4935858", "01030e004da888061a0f70467b5ae891d945336c04516633a43ba12e66add66f", "ab455b003e358dd2879433e2ca23190110407fd8d353acace43d074a3ae8a8c7", "348007a5e01618c5821b9a6feab9095dbebd32893bb93f12f2fd769ba866f8b8"},
    {"er-diagram/visual-check/er-diagram-with-all-relationship-types-neo-look-neo-theme.mmd", 598, "b1259f3604f8a899cc7c0ce1b0e5b0c1a02b10808365e55ffc439533a857ac6f", "d2a10530ef66de97f9445d4d3c178793604bbfb34a69058a4190f587f2a45e84", "01030e004da888061a0f70467b5ae891d945336c04516633a43ba12e66add66f", "3a97ac323b291be504e28eabe9cab0df4d5bac13ff5782c3cf897dc4fe19ae10", "e063c12ec21232932666553b55b191372f6558e81403c178d36437eb9f8e4587"},
    {"er-diagram/visual-check/er-diagram-with-all-relationship-types-neo-look-redux-color-theme.mmd", 599, "c5fc2150294fe4d91d83075d04c5a82dffd688948ae63eadbab520345b4ac727", "c5dd7e740333ae606e2d5cdfa202206ac0c63c22c359944ae56e708bba3180d7", "01030e004da888061a0f70467b5ae891d945336c04516633a43ba12e66add66f", "edcff0717183de8dccd0b13900e26428eb5071982c092dfd6499aad8898ed02f", "293904fdd23b026a13f1a055938ca1981ac6cdd0269e77ba3bae90c2700f413e"},
    {"er-diagram/visual-check/er-diagram-with-all-relationship-types-neo-look-redux-dark-color-theme.mmd", 600, "ffa752cacc9263fbe18085bb6ca7ea0f1ef0b043c427f92fd7c01ba8a6b1c4d0", "fef48b8b6828f43762464458bcfb44779f88a2f92c93b2984833cf1a2bd5b2e3", "01030e004da888061a0f70467b5ae891d945336c04516633a43ba12e66add66f", "69d27e9a286394269a4ec4bfb7df44de1574f84bfe9d0a5f1d5e06329f7d8177", "e23629e3bbb5e9dd73a0ed667195682a396d138a77c589da970c2fa703ed708d"},
    {"er-diagram/visual-check/er-diagram-with-all-relationship-types-neo-look-redux-dark-theme.mmd", 601, "25ed68963c4b1d0570adfafba029aafa5d1f06ea72c8270a7b167e43398e2aa2", "1dfcf0deacac9375e3e0d34936165fc2345656dd0e747dd1863233c2c88c3d70", "01030e004da888061a0f70467b5ae891d945336c04516633a43ba12e66add66f", "34e9102885e118063589030276cb09510f46883916ff4519dddc693b506ce809", "036d37046957faed0a80806a347f874c610b228d2cac1846687975c76a249145"},
    {"er-diagram/visual-check/er-diagram-with-all-relationship-types-neo-look-redux-theme.mmd", 602, "20a70c49acbc27ebf601054fb1d2eedccd2fd4cd3987fde39417c3565ace7550", "78afa0845f6ba24694461dad8f533a77f86b1296601eb96e21b1f59d36561da2", "01030e004da888061a0f70467b5ae891d945336c04516633a43ba12e66add66f", "2d271ff4ae64fffa4daded91829024e1ba36f0501d49ec0e95d37309d1a5397f", "041bbb15423738b05d430de37edba9096ee463a6d0c60540fc1cd69eef9b4804"},
    {"er-diagram/visual-check/er-diagram-with-edge-labels-centered-when-htmllabels-is-false-default.mmd", 603, "89504996e25761dfe406eeb76732c335f840811bcfff7fbc1d104a8508eb91bf", "d025d216f68e34baeeae78b53c7562e9015edce8fdba970e77d752795e68cc4b", "632be3395e75ac8abed875e9e909c5de60643db6448d9f3b68ff98633682b727", "6a9d56eebe6eb4f09498abf1ef4551ce1a11364ade8cb60bfedf3ac75f288d4b", "1346d0884e8a87dae2c09aceb26a9fef9884c7d89be58f818770b8ac20456275"},
    {"er-diagram/visual-check/er-diagram-with-edge-labels-centered-when-htmllabels-is-false-elk-layout.mmd", 604, "85097a641d6e79b915d0cdd636ebe7636a0711c04c8e9c14d0888fb1d853e388", "0091a9e140ec60306afe70db89ef8b8d09578b412ce73d2ec01f003452174754", "632be3395e75ac8abed875e9e909c5de60643db6448d9f3b68ff98633682b727", "a33b68c6a76d3cece00bd274dfa8747c65e94088d1dd76b00bfda2aaa2ed9810", "f9f0879996919417a9f6f54be1aee6fa627d62fdd770e182db5ec8a097dcf358"},
    {"er-diagram/visual-check/er-diagram-with-edge-labels-centered-when-htmllabels-is-false-handdrawn-look.mmd", 605, "180d75ec147d81c4a99e265f5629b1c65aed0d29365a72527fbc4374339b3b18", "83461a93b5a098779b625b3834cc2f21d66b21fa70c8971527e72857b1549360", "632be3395e75ac8abed875e9e909c5de60643db6448d9f3b68ff98633682b727", "64c4be9d23f8438129d6502340d004e36db10356fb87d52cc737b8206f927c7c", "a24dedbba6892e44750fca39ac8e0c88eda3286aa6b0bc10a89e93f7e0bcb768"},
    {"er-diagram/visual-check/er-diagram-with-entities-and-attributes-neo-look-neo-dark-theme.mmd", 606, "9f9a12d4752ff903fc27679b8a39553352141ee3809f51b784344e8434acc4ad", "b7b87d6db1fea5cbec86ec28643bdc4090192f0240ecdc396320b052e3eeb9c3", "ff877eba7f5dfd59328aa48b41ce75ebb056611f89c36f76503f6da0139479cf", "d26717245ecf4021f7b316cecbd15a0ca0d3150c2297fb71169816fafe0db8a0", "ccd3b1c478c60fe45e4f8253be26d400697264d40b813f947d0a7c2c9a54a6ea"},
    {"er-diagram/visual-check/er-diagram-with-entities-and-attributes-neo-look-neo-theme.mmd", 607, "bdf26a124c6b14f456164f3b533bb398e5edd55807ca5e7c147a0c30297ed0dd", "b8587a7a4e59abddefbe13c166a122e45b07a8ecb4e8c246106e1281ec13d162", "ff877eba7f5dfd59328aa48b41ce75ebb056611f89c36f76503f6da0139479cf", "4bbff3a9aa7670c2969ff1912276c7c087bdb7ff96ca097b8cd7387eb3cc08da", "35991afe704f22053034cdd5b9c930b7121285828f105a0c77d8fcb9cbb8a12a"},
    {"er-diagram/visual-check/er-diagram-with-entities-and-attributes-neo-look-redux-color-theme.mmd", 608, "fb2f343e380fb43f580e6138b0223a5d4bf57d2ead250da428e79f85960de382", "f27eeaab48d4bf1d6e4e4200153ff36d03740f0e2de661e300386f9794fcfb06", "ff877eba7f5dfd59328aa48b41ce75ebb056611f89c36f76503f6da0139479cf", "3d88b0eaa37daad87eca1d2ecc208882abf42ccf625bc7e5e83b93076d5d7e31", "5c9d1653839c73f90e2b531cc2b470ce74f14ccbb403390993c949f2eda6c230"},
    {"er-diagram/visual-check/er-diagram-with-entities-and-attributes-neo-look-redux-dark-color-theme.mmd", 609, "1f65b2cc20aaadc76ce337e5733afb5810285aee97c45921bbdb8e527c25eec2", "8490d4635673aae74b5e08b25158d4ef10257cbf804cb45c8468f0c1aaf6651f", "ff877eba7f5dfd59328aa48b41ce75ebb056611f89c36f76503f6da0139479cf", "a83f045fcfb2e665df32b8f359780822a33038ea58d125f54ab0832db019f0d0", "482356396ea7d0dc0561791d2d3150bb6d18624ba1b02f16f6cbac550f346ee5"},
    {"er-diagram/visual-check/er-diagram-with-entities-and-attributes-neo-look-redux-dark-theme.mmd", 610, "0a7ee5101ef3fa1097092e6039ca64dfe02ab1b172d42b039dc5ab4470e7ca55", "331c8b8f4412fdb55bda854b37c529fa57025a0b77a9967a5418ebed059855c3", "ff877eba7f5dfd59328aa48b41ce75ebb056611f89c36f76503f6da0139479cf", "19a30e9555b3e583c9eafb3c6280ca4955a47af437a6b92997ec0f46f0943483", "5a4050d472afb026edf9f69c0e78685294aa1b03d6c922b2fe5557c80fda1145"},
    {"er-diagram/visual-check/er-diagram-with-entities-and-attributes-neo-look-redux-theme.mmd", 611, "0d3fb3ce8497dd8a365c77bf09ecec607e7dfd0d6040c56e93357d9781ad132e", "cf6045383c26eb540767d27a1d083b296486059137f2d57fc1df4d8f091823a3", "ff877eba7f5dfd59328aa48b41ce75ebb056611f89c36f76503f6da0139479cf", "d34226f861737e00d6bed10b00ce5883a083e63a9f88bb3f968bd45930588755", "9474a6cbbed9099b472578f96a9d84b7f4e7dc6d7f7fdc3116ae494cb51beff1"},
    {"er-diagram/visual-check/er-diagram-with-entity-aliases-neo-look-neo-dark-theme.mmd", 612, "4d23c1f7e93f475f26ad5ecf8a86b2f068f4f97a91af98a7e82df66b2a0d43fe", "e3f3e5cd658045eb79ca728b8c3cb55f17bba229464f7dd98111bd1530ebbc5e", "1cdc1de6dc2a6a1fa47c9501125d8f1c518f9d5c40d266aa2d6e7c220c555326", "3a63b6e5d894c6e958f60c245796ff037a1c3cf9a624803c985d7480fdde1fcb", "97340f998b4e927e91c17213ad8b1ef8451adc952bd972acc26cf489cca4d5d5"},
    {"er-diagram/visual-check/er-diagram-with-entity-aliases-neo-look-neo-theme.mmd", 613, "0d37d38df2b2cc580ba8d913bc0e9bbdbc4a940bd4b71136a043aa147325b7b7", "7692825bf1924969b39d6d4d1f8843bc1211ae2ec9d8b14d9bbcef1ceb075ee6", "1cdc1de6dc2a6a1fa47c9501125d8f1c518f9d5c40d266aa2d6e7c220c555326", "326cc922517386a9575df4096f0aff9429847e8245ca390ed47ef89a5bfef10d", "4184f303bc8ee4c50f3f35ceca64ab32979f6c20738f6221fe3be44fc0b368de"},
    {"er-diagram/visual-check/er-diagram-with-entity-aliases-neo-look-redux-color-theme.mmd", 614, "f676cb85d8109099bcae092e72aa670d3c7efb7189ddc2eef9a6f45262cbda9b", "e9f7425a2e4929f886e189d04e7409fb94258e7076194aa6d511befde8a702fc", "1cdc1de6dc2a6a1fa47c9501125d8f1c518f9d5c40d266aa2d6e7c220c555326", "abe0b3e00ffe43dda72bc93bae02670d039589ea81e699476b8893946eb437d7", "90b661269f59b5a6f157db9fd9199b92bf464acf9c222de16bc1c2fa72fe6adf"},
    {"er-diagram/visual-check/er-diagram-with-entity-aliases-neo-look-redux-dark-color-theme.mmd", 615, "bb4d305134ca67ddb4b0f68f7a0531625f07dab6b31021cdbcc8c669709d94db", "c54659e7cd323f085548194a189cb8a1ff683838d3cb5a4a098d919ade1cfccf", "1cdc1de6dc2a6a1fa47c9501125d8f1c518f9d5c40d266aa2d6e7c220c555326", "aaa6d71e30dbb1f40a0cf218b537dd6bef54727467fcb0a986f6e27bbc440133", "a70f9a43b3c72a8a302a6a36be1b4db1073682ac888f98eddd08423019d80ffd"},
    {"er-diagram/visual-check/er-diagram-with-entity-aliases-neo-look-redux-dark-theme.mmd", 616, "9bf67352e2ec0a6dfcf9ed054bd21f1c855dbfe2aefe98d6f06001aabf53119e", "000dd673e41188b1f9fcd0c1aec1eb86470985636c70b56ecf7327766231a09f", "1cdc1de6dc2a6a1fa47c9501125d8f1c518f9d5c40d266aa2d6e7c220c555326", "f9167ceec7abcfff0e58326704b3c16fc40ad99c9488f2044e8af52eb04aaca5", "77a0c33f1cf2ae42666ad15c27c973f050806db34eeeb8e2cf60bfb3cafc0658"},
    {"er-diagram/visual-check/er-diagram-with-entity-aliases-neo-look-redux-theme.mmd", 617, "dd13277e11fd2ec17a0ff200d90455f091c9d26e85ca18562600aa1c8eb6a284", "ba83330fcb5be90556860a27383596f999a6af31b9171197df3ee541635e68de", "1cdc1de6dc2a6a1fa47c9501125d8f1c518f9d5c40d266aa2d6e7c220c555326", "b6f06bebaef801c591de3d0588504c1e3980f1a18b5cc140ed707cad84e0d1b0", "f895851fd97554a02f63780bfafdc4ccfa33dac2af62181bef80fb14af89ef56"},
    {"er-diagram/visual-check/er-diagram-with-keys-and-comments-neo-look-neo-dark-theme.mmd", 618, "7aefb33847ce827ef2aa1d78ea6bcc770397032d77013991627588bc962cea14", "4d088a851c1a40034e1c32c207206d671dd497ca4a1a0df9671e413eaf68445d", "d4b180dadac7fc1756d00a7996cef5e4ccbfbbc43ec9e4667ab5a7ad9007c45e", "4bd2ee9dbbf5c7977d84b38b12f7fafc096246c9d0f074c635c851b0242f7994", "868f2d33d699d6a6a7cfd09851d4c8b0ec1e047b736749df5fd9a24bfb815b15"},
    {"er-diagram/visual-check/er-diagram-with-keys-and-comments-neo-look-neo-theme.mmd", 619, "efc9583434bbec5f4c48f8061598f624fd88489102092e3f120e900818bae112", "7fa2b941dc22e4a18a73682fe3b80820b911b356363bc5d647e9fcaa975470b6", "d4b180dadac7fc1756d00a7996cef5e4ccbfbbc43ec9e4667ab5a7ad9007c45e", "7d2d48bc40a29fbacc918e0a9ba2c027ad06ba8d6e91f93192591344f9b6f465", "4a92736236ae97a36c453986bb5834c98c2666cfc3cd0b19545fe0032e67a8f7"},
    {"er-diagram/visual-check/er-diagram-with-keys-and-comments-neo-look-redux-color-theme.mmd", 620, "21b1a8ec9e9611c269a893d4ad9179fc636e0d4d58dd7cd4059c671b9d239575", "07f676a74440a0f36a9f4c876457745bd054f2a742e7c3d77c70bdb38dc55b88", "d4b180dadac7fc1756d00a7996cef5e4ccbfbbc43ec9e4667ab5a7ad9007c45e", "712fd31b9820b4b7a7b49d01fe4b36f1981fe5095d36a77d92f83775fba00dc0", "5a8d3601051ba39d897339774dc8b5a5a71312c37709fe2960a24909fa100a4e"},
    {"er-diagram/visual-check/er-diagram-with-keys-and-comments-neo-look-redux-dark-color-theme.mmd", 621, "31ebd63c29aa05822445a9d2e942fd3e51612a2204058796af482df23d9994fb", "1c053f7fdde10e30eea63e25ea4cd4cbe08a0f46b100603da34fcc420cd5d5d8", "d4b180dadac7fc1756d00a7996cef5e4ccbfbbc43ec9e4667ab5a7ad9007c45e", "8f84b88aa11ba54543833c2c1a4d8593843cdac21510a2cc53de008af0f7f2d8", "e474e4cccf0dea3aafa47377754c93ccfe9462ef0d15eee79d1fc3a8d2f60be7"},
    {"er-diagram/visual-check/er-diagram-with-keys-and-comments-neo-look-redux-dark-theme.mmd", 622, "d9b059bdd477a2f25c6b7e91c7946a19edac7870e3a0171490ec4e2b4d6f76dc", "6cdadd633715f05643447250ecf3df06a775273dee2d89be5c3a6b31022a74e6", "d4b180dadac7fc1756d00a7996cef5e4ccbfbbc43ec9e4667ab5a7ad9007c45e", "7acfd50f339eae437f8272b624b56296ab824759c99c5e14ab84e198b3b6bc8f", "53689104f01abc224131488792f510e8eaa047c7ab71d41bb0c4e9d9e90e26ef"},
    {"er-diagram/visual-check/er-diagram-with-keys-and-comments-neo-look-redux-theme.mmd", 623, "40afa87f62d9381e6a7ce913b0d66a29186ca58af61851d5c534e1a49cf7c999", "8cecd69b45b0d25a55e490e755c6fb2cbab45860e30a151ac5d06026be8a5ae3", "d4b180dadac7fc1756d00a7996cef5e4ccbfbbc43ec9e4667ab5a7ad9007c45e", "9a94fc702652a4b4225b98b8a27e79a7d34b70953ea19922558b872303b54f70", "de08071d1c0b74d9bdb86e6cbdf5497ebd13db3c2a3f9106fd3e0f9a5e0eb0ef"},
    {"er-diagram/visual-check/er-diagram-with-recursive-relationships-neo-look-neo-dark-theme.mmd", 624, "bb4dd92cba370eaeb7b9a7eb1177beed0361b026940c4f2f24729174bf4f9b87", "00ec812b440121a12dbf8310c429f5ca206206f5ff1ba3d2dda27ce4fdb1906a", "8c78c56251514ba2b530933b48447fac1017df8b38d5f6b3af4d8c478ce6ab32", "3759356d73474af4512eac29964e859aec0ac4cfc42dc709b7148bdc95da3b6f", "54234e6d35fe11b86d19eb4c5892c620ecaba70eadbf93bc5746002359016796"},
    {"er-diagram/visual-check/er-diagram-with-recursive-relationships-neo-look-neo-theme.mmd", 625, "c9a9656337c2dc43a4c296a04fc19024aa251af8286f2d9a4efd3f81a2407a15", "3dc28598dc39c360f5eb2f8847d57f95166570a308c6f2abc57ed601fa9a1681", "8c78c56251514ba2b530933b48447fac1017df8b38d5f6b3af4d8c478ce6ab32", "cc1cfb2892b5d6a467a4ad82c3110d2f128655c0551859805e4d6773287e6ea4", "15245329beb4320162422d22c800cc6cc6307dc039e09517857777de2d2bfa5e"},
    {"er-diagram/visual-check/er-diagram-with-recursive-relationships-neo-look-redux-color-theme.mmd", 626, "83508559499037ba6b3eff8aed516529776c1e7a9d379f7f59f13f2df007f052", "b09f21420c8d813ee3ac45659579aa7c628680bfed3a4dcfc0dcaff48012d402", "8c78c56251514ba2b530933b48447fac1017df8b38d5f6b3af4d8c478ce6ab32", "a74dc76e7ac6de7ff2ee36f16ec5c33f14cf58654af04e3eeff2126cbe213d76", "82624fb8962f22a3d5c82271f8182665fde6a8c046c7217745cd5058a62ce7a9"},
    {"er-diagram/visual-check/er-diagram-with-recursive-relationships-neo-look-redux-dark-color-theme.mmd", 627, "878e9207127cbbb6bbd77e84837c4e9f7518d9e5a85efc7deebedf8704340482", "0bd6c916f6429e3ef0817a33033c370a4cfa4ebea22542c043b90a53cbf9f873", "8c78c56251514ba2b530933b48447fac1017df8b38d5f6b3af4d8c478ce6ab32", "37c6c7c1e71b29fea809176c863770dad2ede858a72ff3ed41df4af715bf7850", "ecc88c0354807a9a5b68bb2996f12bc66bfb70fe909b9c4895cf45b7f78708fc"},
    {"er-diagram/visual-check/er-diagram-with-recursive-relationships-neo-look-redux-dark-theme.mmd", 628, "771ef96089f28ab1ce745be46d403db1efd3cc74f2fd96e62093b7992e108a41", "b08597b5ae5d142de40eac9fba56e0ae43f64f07003c1e3b9b34c22c9a8eee5f", "8c78c56251514ba2b530933b48447fac1017df8b38d5f6b3af4d8c478ce6ab32", "0e1d9e02e22f2836ab12333363e18d081e5626338ca980204ab2c79b72392fe3", "eeb099a0a1ff470ef8d58addcc21e70bd7d5f0e9c7128b84c53af2a13c524210"},
    {"er-diagram/visual-check/er-diagram-with-recursive-relationships-neo-look-redux-theme.mmd", 629, "2c0614c09ba2bcb0a75a5965bf36fb1be842ecb1dcac03e906bfde5ea57866b3", "e52b675614b5021f1be21b7b8e8e1002499de67265df2b454f13c25fb0b8d7d7", "8c78c56251514ba2b530933b48447fac1017df8b38d5f6b3af4d8c478ce6ab32", "c7486e11197726437b379e52fb6da7393e5b6392728ed1247c37b40ef1e42d30", "cffd12657d2a8332d38f87356467b85fce67cf5149d88ab3a3d743ecebad9f24"},
    {"er-diagram/visual-check/er-diagram-with-standalone-entities-neo-look-neo-dark-theme.mmd", 630, "6229c058926443bf9e7dc4881501d7f98810496fd815911072c23be492a44153", "ba217155115ee83e8bd54502e4590ad6a2b95d1b6fddee31e3170b603a3f7027", "cf34c622afad80fb60f39646499f4efbe972c0d3f2f23608df908d82ebe06ab3", "97f1210070f4e404f2170b003e9381d21fb483d3f4ae26f121d27a6d69bf19b0", "92d1fdda7a18f8669a7a6fb6d28a642f4d23cc1fcfa6e55143fc6990f9147028"},
    {"er-diagram/visual-check/er-diagram-with-standalone-entities-neo-look-neo-theme.mmd", 631, "2e85cf2abc09f71887499bdf1a950acf88faae61ed6386035b2c58317f220f52", "21ceae92a552b9003d23eea7b9227405c47d0476c206c5fde7eaf8acf6d2d75e", "cf34c622afad80fb60f39646499f4efbe972c0d3f2f23608df908d82ebe06ab3", "f4465c2a63e17a2cf0eed418128a90f3d6b189d09b51fbf0e48261fd53762346", "33abe891fda9920d86aa084967a85d9e53db2bc43df38b263ae45258a68a4ec0"},
    {"er-diagram/visual-check/er-diagram-with-standalone-entities-neo-look-redux-color-theme.mmd", 632, "811007b475a8f871e333bc153c9c8a7be976d0f3f6e7093f5f15dc3414f36ae1", "7a55e97fbc7d59a4703ecc547dd4dd708ae05eb8751aaf13162a4407a75bf074", "cf34c622afad80fb60f39646499f4efbe972c0d3f2f23608df908d82ebe06ab3", "fd7315966660e64e315420874a36157c905dd27b6b44738566ed9eef1b524fbf", "45de81112227f7a1b3bfd99476b3de146d3fefa01eb9b1c1ff0284b7859c9435"},
    {"er-diagram/visual-check/er-diagram-with-standalone-entities-neo-look-redux-dark-color-theme.mmd", 633, "46be73cb172f9caf49653abf1d54d902c7b77ebced5d4544cfed6da142bbdb47", "f42045bf173f551f5c4064ee591d436c3b3943dd69629f3cb6014db5e2ac039c", "cf34c622afad80fb60f39646499f4efbe972c0d3f2f23608df908d82ebe06ab3", "74ecd65c14a64d6850742d51503ecfe881c55b9a44003a7e1f31ecb628a328b9", "597888886cce129d01cceaec9326c95cb871f7f6dbebc5f5f047db33d244b035"},
    {"er-diagram/visual-check/er-diagram-with-standalone-entities-neo-look-redux-dark-theme.mmd", 634, "63f728097bc5387ab77c3de61cb20f7469c4464ad1dfa2df14a2480a5d8f8129", "16b30a635627ca8a8e77806f5a3ae5e45e330506e71fc15729384c7f056de7c8", "cf34c622afad80fb60f39646499f4efbe972c0d3f2f23608df908d82ebe06ab3", "ebbda5d8ed9b05dd9fa21a8dc2f89de2008566d67001c09ab121d8bca8b47285", "86852764afc981f7cae85d47f9656d86c9ced2c197a5d21d195647a65a6aa4f3"},
    {"er-diagram/visual-check/er-diagram-with-standalone-entities-neo-look-redux-theme.mmd", 635, "714da4bad155ccd6b0e42c456d748fc824882ddc1777042db594f57c21bbdedb", "cc81392943cef0ff4f056047fb18538aff1b62935b60e07b3bcb0e080f2bbeec", "cf34c622afad80fb60f39646499f4efbe972c0d3f2f23608df908d82ebe06ab3", "09d880b1da468977dbb30d61509d93d22c2585f0d7bf48469e578a12a7d283b0", "37aaa39b4f9556a6404af85059fef39482310e325c3381d3ece2e6a6254ca04c"},
    {"er-diagram/visual-check/er-diagram-with-various-attribute-types-neo-look-neo-dark-theme.mmd", 636, "30182da9ced48ea2a3a78117f06b386a07359580d5b9fc87c3db8d99f7cadb37", "7732c0f028a6499affb02da5faf4d8ab940c83d12cec27fdccee0c75313a6587", "41637199446e76253a258e09452c88d9dc6b152efe7a951b4a8d30843518f25d", "a4dd67fdba19aea93e5ef5db84273d986af97a2711df8484bcacc786830872ad", "19077abaf2d7a4a8a8fbd6138a9ecae9cc44e183ab58c96bf435a439be77c660"},
    {"er-diagram/visual-check/er-diagram-with-various-attribute-types-neo-look-neo-theme.mmd", 637, "77d459e5180a663e7915a51e64a8bb0bc734fcea86a3b0fa31f93f72cd08ece1", "d01d075cd678b287ccce9b4a5a541cee9e2f97ad2f4282e94163311626dab608", "41637199446e76253a258e09452c88d9dc6b152efe7a951b4a8d30843518f25d", "5071def17ac7eb9b63eadc021633d66435d884b6ed94ed45b3c6c28dee3feb29", "1ba05fa5cb1867aa7a0dc4c685ca69b6f7c94cc04c0b56ef9c959a3e1a40e3b0"},
    {"er-diagram/visual-check/er-diagram-with-various-attribute-types-neo-look-redux-color-theme.mmd", 638, "733c5bbc08741ec8cc5ed889d8e20782d1de2a6630142137b7a34944cc25d16b", "aeadaaf5e6d9b0005f38324511f0b36fc8963c0ce9fefcde4755066c066cce2d", "41637199446e76253a258e09452c88d9dc6b152efe7a951b4a8d30843518f25d", "36263eaeac4a8d0db7a0d0e584c67ed6985acccae6dfd496707ff717312ca883", "6fac0074a8040c70c4d010a34a549720b44e572c276f9d827edc61f38e802145"},
    {"er-diagram/visual-check/er-diagram-with-various-attribute-types-neo-look-redux-dark-color-theme.mmd", 639, "1888c9187792eee14751096e591a0a7c93a871678640c29993d1d19916e14db3", "0e692059c8e8a6d43edf63f9e7f3d411cad24dbaec0ddc1e2f90235705161b0d", "41637199446e76253a258e09452c88d9dc6b152efe7a951b4a8d30843518f25d", "afabef797059e84a8782cf1809ec674f6d4fd5c552e9b6534acb8a8424fb89c2", "3d58d2b50e45426436c52db727393545780b6a75c4f05ae5c61f29cb93184916"},
    {"er-diagram/visual-check/er-diagram-with-various-attribute-types-neo-look-redux-dark-theme.mmd", 640, "7f811b13f5925aaf01b6e04ac25f66a3d0b1a3eb3095131933313b2cf69c8a13", "e85191af8643f6ebec3ebe3a811ee1272c867d53c06eba0cca32436cc1426531", "41637199446e76253a258e09452c88d9dc6b152efe7a951b4a8d30843518f25d", "564be38c9716e3c55a828b894208e055115ca2204bbfae4351f1a0e87c2cd60e", "1fe2549d7d72c03d93fdd23ff3bb9bae6bd4787ec4a0f9284563095732c6e259"},
    {"er-diagram/visual-check/er-diagram-with-various-attribute-types-neo-look-redux-theme.mmd", 641, "d3032375b710c22638f50822e422c63104fab190f9d70935b136618d5106ed59", "28c1c07a0c3029417cb6b084fb0b11f9b8b6508d7ddd59119a6d789dccdb89b1", "41637199446e76253a258e09452c88d9dc6b152efe7a951b4a8d30843518f25d", "36b81550027b98f0717bd086590a3c7c28cda14c9e90020fa5ec74a7eeee1474", "4906fba9bb7640c60134d3009e247fc153dfa85a95dd0fb24fd8cfaa155b0de7"},
    {"er-diagram/visual-check/er-subgraphs-handdrawn-look.mmd", 644, "bf4856327640454877c0be3e897491792437d42cd2cde70bfa3fe10c77193789", "e7875fe1eaf4f5ea2c4c24c5ce3862e0eef1d0c41e33967714b2097385081908", "0ec20a9ce75327783132705ae3e5e240f92a4b5c83ba2cd2826e15fc0c6c27fc", "6683b190193fae0a9b77717be6fc3d83b3b7867324db8b06491001a93708ff32", "5441a2ba801b3ff5dd0eaf5b1f20308117adde6545f4f6acf3e96a10d20cd72c"},
    {"er-diagram/visual-check/simple-er-diagram-handdrawn-look.mmd", 651, "38e09baa3629371a83d68507e897db97bacc577679e088ca21ad830d98ad63dc", "e0e5c91758c5889510b5b050545e5d194a9a23158b17daf5c1bd2a7d98b8990d", "7cb766efb54274d728e696aa24c7f72ebbdfd453e8fc0b983d2fd46b0b311d68", "0c25409e723bf2b08442a3e97f1d0fc400c41b0fa3d34107777b2d064188bfde", "7c7f8ffd44b2c64752d0bc99e7d92b0d00e4806c7cf7b0774627f508649e0afd"},
    {"er-diagram/visual-check/simple-er-diagram-without-htmllabels-default.mmd", 652, "9d847d9fe6c3707e638dc1333c1746f67ed9405201081b2bfca84eeae7dd9b4a", "06951ed74a852d5b5d14faf28354cbb71f2a2380b6d92bdfa54fc40a79be0880", "7cb766efb54274d728e696aa24c7f72ebbdfd453e8fc0b983d2fd46b0b311d68", "9c5731474c896ed5b7e45ac2d19393972ae8aafdfd6fbd228e824ace6b5d420c", "c1fa5def35b984dc35c556f26b9a8c1e8948a5ec050f8534b6d333ffe5beb3ea"},
    {"er-diagram/visual-check/simple-er-diagram-without-htmllabels-elk-layout.mmd", 653, "59c2a7a5ee3f840eacfd3f0a5b7b7a27f4680d613eaf6ed704b8f5ea052130c8", "a0b8dfab05a8bba864f450d6e6fb4e66cb4c80bfb8816774dd5e7573ae1bd4a8", "7cb766efb54274d728e696aa24c7f72ebbdfd453e8fc0b983d2fd46b0b311d68", "05c2ff304c045f57beac038552c424d3a4add5b768dea85e29d15b42969649ab", "dc30d478a1591e53153657b84d917c9646e1cbe4060431bdfd57934f8ede80fc"},
    {"er-diagram/visual-check/simple-er-diagram-without-htmllabels-handdrawn-look.mmd", 654, "825b85bcd07dfa72d63d042b658620c8e1c2c097513cca9abc0712a9764fe31e", "70c59df921661bfdcd0d2ad25042f6ca4bfa0ba041cc88399827662f8119fbed", "7cb766efb54274d728e696aa24c7f72ebbdfd453e8fc0b983d2fd46b0b311d68", "8af19172c7264d8d578850d1c7a2bd58ce68d84502b75ee073b12be5e761bc6d", "3d87cae1e2ba860475208e6a6ea240dfdf4014169540fc6710a6d3704c5760d7"},
}};
inline constexpr std::string_view kSharedSha="e6ac9b68c3dd5a49608692c3b7019da5ec72c74bd30858c423d3d0ced4e017f0";
inline constexpr std::string_view kPaletteSha="fd58945ec9fc772ae0986f286afb4ab7cd522c1ec72d71b053b8c48517d23a33";
inline constexpr std::string_view kPresentationCanonicalSha="301cce1af96eb0125d632ccc73418bd8db2a5b7850d3b83ae78fa57c9248265c";
inline constexpr std::string_view kContractBasisSha="7f8d1a93b064a862dad43c695fd02547f093762d8e5f09ddce6aa3570dd4c3fe";
inline constexpr std::array<std::pair<std::string_view,std::string_view>,7> kComponents{{
    {"additional_upstream_evidence_sha256","34ddfbef0f15d9c527b2bd6d1d5309116bea1d28f863d475ba0b36421142d75d"},
    {"complete_models_sha256","b292027c51dffeed9dcdc3d5d954aea2fc04bb15ee85a1483a95e465d82ce100"},
    {"palette_source_and_projection_sha256","fd58945ec9fc772ae0986f286afb4ab7cd522c1ec72d71b053b8c48517d23a33"},
    {"source_bindings_sha256","6d1826bb47c885d75b702ae86dc7aa2e2a1f1c0b249458fb85794085c4e315a0"},
    {"terminal_models_sha256","ead3ce2dcd985dda285d8d178d367b3c36ee0e750536c54b5c95fd6630f4aa8a"},
    {"upstream_evidence_references_sha256","91f6c21e275fe52d9b9e7a3ff45a1b15af3d16d548d12b97d87f4ab0354c0031"},
    {"whole_scene_models_sha256","93314b3eb0cb018f327e56167225af713b9196bdd5a93d9832302352fd3f2352"},
}};
inline void require(bool good,std::string_view why) {sd::need(good,why);}
inline bool resource(std::string_view why) {
    return why.starts_with("resource:")||why.find("exhausted")!=std::string_view::npos||why.find("limit exceeded")!=std::string_view::npos||why.find("bounds exceeded")!=std::string_view::npos;
}
inline std::string text(JsonVal v,OracleWork& w,std::size_t limit=65536) {return std::string(sd::text(v,w,limit));}
inline std::string canonical(JsonVal v,OracleWork& w) {return sd::canonical(v,w);}
inline void exact(JsonVal a,JsonVal b,OracleWork& w,std::string_view why) {require(canonical(a,w)==canonical(b,w),why);}
inline void verify_asset_bytes(std::string_view source_bytes,std::string_view projection_bytes,
    std::string_view presentation_bytes,std::string_view manifest_bytes,std::string_view prior_bytes,OracleWork& w) {
    source::verify_asset_bytes(source_bytes,manifest_bytes,prior_bytes,w);
    for(const auto& item:std::array<std::pair<std::string_view,std::string_view>,2>{{{projection_bytes,kProjectionSha},{presentation_bytes,kPresentationSha}}}) {
        require(item.first.size()<=source::kMaxFactBytes,"resource: ER107 projection asset4MiB");
        require(sd::digest(item.first,w)==item.second,"source: ER107 projection/presentation bytes pin");
    }
}
struct WrappedRow {std::string text,joiner;bool operator==(const WrappedRow&) const=default;};
inline std::vector<WrappedRow> wrap(std::string_view value,OracleWork& w) {
    require(value.size()<=65536,"resource: ER107 display scratch64KiB");
    sd::pay(w,value.size(),6,128);
    for(const unsigned char c:value)require(c>=32&&c<=126,"protocol: ER107 preflight ASCII display subset");
    std::vector<WrappedRow> out;out.reserve(value.size()/16+1);
    std::size_t at=0;
    while(value.size()-at>32) {
        w.spend(40);std::size_t space=std::string_view::npos;
        for(std::size_t i=1;i<=32;++i)if(value[at+i]==' ')space=at+i;
        const auto end=space==std::string_view::npos?at+32:space;
        out.push_back({std::string(value.substr(at,end-at)),space==std::string_view::npos?"":" "});at=end+(space==std::string_view::npos?0:1);
    }
    out.push_back({std::string(value.substr(at)),""});return out;
}
inline std::vector<WrappedRow> checked_rows(JsonVal value,std::string_view display,OracleWork& w) {
    const auto want=wrap(display,w);require(value.is_arr()&&value.size()==want.size(),"source: ER107 complete wrapped row count");
    std::size_t at=0;
    for(std::size_t i=0;i<want.size();++i) {
        const auto r=value.at(i);sd::keys(r,{"consumed_byte_end","display_byte_end","display_byte_start","joiner","text"},w);
        require(text(r.get("text"),w)==want[i].text&&text(r.get("joiner"),w)==want[i].joiner,"source: ER107 exact display row/joiner");
        require(sd::integer(r.get("display_byte_start"),w)==static_cast<std::int64_t>(at),"source: ER107 display row start");
        at+=want[i].text.size();require(sd::integer(r.get("display_byte_end"),w)==static_cast<std::int64_t>(at),"source: ER107 display row end");
        at+=want[i].joiner.size();require(sd::integer(r.get("consumed_byte_end"),w)==static_cast<std::int64_t>(at),"source: ER107 display joiner end");
    }
    require(at==display.size(),"source: ER107 exact display reconstruction");return want;
}
inline std::string generic_display(std::string_view raw,OracleWork& w) {
    require(raw.size()<=1024,"resource: ER107 raw type bound");sd::pay(w,raw.size(),8,32);
    const auto identifier=[](std::string_view s) {
        auto first=[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';};
        if(s.empty()||!first(static_cast<unsigned char>(s.front())))return false;
        return std::ranges::all_of(s,[&](unsigned char c){return first(c)||(c>='0'&&c<='9');});
    };
    const auto a=raw.find('~');if(a==std::string_view::npos) {
        require(!raw.empty()&&std::ranges::all_of(raw,[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||std::string_view("_-.(),[]").find(static_cast<char>(c))!=std::string_view::npos;}),"source: ER107 plain Markdown/HTML-free type");
        return std::string(raw);
    }
    const auto b=raw.find('~',a+1);
    require(b!=std::string_view::npos&&b+1==raw.size()&&identifier(raw.substr(0,a))&&identifier(raw.substr(a+1,b-a-1)),"source: ER107 finite whole-token generic grammar");
    return std::string(raw.substr(0,a))+"<"+std::string(raw.substr(a+1,b-a-1))+">";
}
inline std::string role_display(std::string_view raw,OracleWork& w) {
    sd::pay(w,raw.size(),3,16);const auto a=raw.find_first_not_of(" \t\r");
    return a==std::string_view::npos?std::string{}:std::string(raw.substr(a,raw.find_last_not_of(" \t\r")-a+1));
}
inline std::string multiline(const std::vector<WrappedRow>& rows) {
    std::string out;for(const auto& r:rows){if(!out.empty())out+='\n';out+=r.text;}return out;
}
inline std::string marker(std::string_view card) {
    if(card=="ONLY_ONE")return "card:one";if(card=="ZERO_OR_ONE")return "card:zero-one";
    if(card=="ZERO_OR_MORE")return "card:zero-many";if(card=="ONE_OR_MORE")return "card:one-many";
    throw std::runtime_error("source: ER107 cardinality enum");
}
inline int hue_role(std::string_view hue) {
    if(hue=="Neutral")return 1;if(hue=="Red")return 2;if(hue=="Orange"||hue=="Yellow")return 3;
    if(hue=="Green")return 4;if(hue=="Cyan"||hue=="Blue")return 5;if(hue=="Purple")return 6;
    throw std::runtime_error("protocol: ER107 hue enum");
}
inline std::string rgb_hue(std::string_view raw,OracleWork& w) {
    sd::pay(w,raw.size(),8,128);while(!raw.empty()&&raw.back()==' ')raw.remove_suffix(1);
    require(raw.size()==7&&raw.front()=='#',"source: ER107 pinned RGB hex grammar");
    auto hex=[](unsigned char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;throw std::runtime_error("source: ER107 RGB hex digit");};
    std::array<double,3> c{};for(std::size_t i=0;i<3;++i)c[i]=static_cast<double>(hex(raw[1+i*2])*16+hex(raw[2+i*2]))/255.;
    const auto lo=std::ranges::min(c),hi=std::ranges::max(c),light=(lo+hi)/2.,delta=hi-lo;
    const double sat=delta==0?0:delta/(1.-std::abs(2.*light-1.));
    if(sat<.15||light<.10)return "Neutral";
    double hue=hi==c[0]?60.*((c[1]-c[2])/delta):hi==c[1]?60.*((c[2]-c[0])/delta+2.):60.*((c[0]-c[1])/delta+4.);
    if(hue<0)hue+=360.;
    if(hue<20||hue>=345)return "Red";if(hue<45)return "Orange";if(hue<70)return "Yellow";
    if(hue<165)return "Green";if(hue<195)return "Cyan";if(hue<255)return "Blue";return "Purple";
}
struct EntityRole {
    std::string id,owner;int foreground=1,fill=1,source_index=0;std::optional<int> slot;
    std::string border_hue="Neutral",fill_hue="Neutral";bool operator==(const EntityRole&) const=default;
};
struct Projection {
    relation::Model scene;
    std::vector<EntityRole> roles;
    std::vector<std::string> notices;
    std::vector<std::vector<WrappedRow>> notice_rows;
    std::string theme,look;
    bool categorical=false;
};
inline Projection decode_semantics(JsonVal terminal,JsonVal source_model,JsonVal palettes,OracleWork& w) {
    const auto encoded=canonical(terminal,w);sd::pay(w,encoded.size(),8,8192);
    const auto nodes=source_model.get("entities"),edges=source_model.get("relationships"),groups=source_model.get("subgraphs");
    require(nodes.is_arr()&&nodes.size()<=64&&edges.is_arr()&&edges.size()<=128&&groups.is_arr()&&groups.size()<=64,"resource: ER107 semantic object bounds");
    const auto meta=source_model.get("metadata"),cfg=meta.get("authored_config"),tm=terminal.get("metadata"),wm=terminal.get("whole_scene_model");
    require(terminal.get_string("review_status")=="UNREVIEWED"&&terminal.get_int("drawing_credit")==0&&terminal.get("serialized_body_coordinates").is_null(),"source: ER107 proposal status/zero geometry credit");
    require(tm.get("description").get("visible").is_bool()&&!tm.get("description").get("visible").as_bool(),"source: ER107 documentation-only description");
    require(tm.get("description").get_string("source_value")==meta.get("description").get_string("source_value"),"source: ER107 complete hidden decoded description");
    exact(tm.get("authored_config"),cfg,w,"source: ER107 authored config");exact(tm.get("ordered_config_leaves"),meta.get("ordered_config_leaves"),w,"source: ER107 ordered config leaves");
    require(tm.get("source_effective_config_remains_unknown").is_null()&&tm.get("title").is_null(),"source: ER107 foreign effective config/title");
    Projection out;out.theme=cfg.has("theme")?cfg.get_string("theme"):"redux-color";out.look=cfg.has("look")?cfg.get_string("look"):"neo";
    out.categorical=out.theme=="redux-color"||out.theme=="redux-dark-color";
    const auto eff=tm.get("proposed_fresh_owner_effective");
    require(eff.get_string("theme")==out.theme&&eff.get_string("look")==out.look&&eff.get("categorical").is_bool()&&eff.get("categorical").as_bool()==out.categorical&&!eff.get("host_initialize_reproduced").as_bool(),"source: ER107 explicit fresh-owner defaults");
    relation::Work typed_work;out.scene=relation::er296_typed_model(wm,typed_work);
    require(out.scene.family=="er"&&out.scene.direction==source_model.get_string("source_db_direction_under_fresh_instance")&&out.scene.entities.size()==nodes.size()&&out.scene.relations.size()==edges.size()&&out.scene.groups.size()==groups.size()&&out.scene.notes.empty(),"source: ER107 complete typed scene population/direction");
    require(terminal.get("entity_presentations").size()==nodes.size()&&terminal.get("relationship_presentations").size()==edges.size()&&terminal.get("group_presentations").size()==groups.size(),"source: ER107 complete presentation populations");
    for(std::size_t i=0;i<nodes.size();++i) {
        w.spend(256);const auto s=nodes.at(i),p=terminal.get("entity_presentations").at(i),n=wm.get("entities").at(i);const auto& e=out.scene.entities[i];
        auto label=s.get_string("source_db_label");const auto aliases=s.get("alias_attempts");
        for(std::size_t j=0;j<aliases.size();++j){w.spend(32);const auto a=aliases.at(j).get_string("value");if(!a.empty()){label=a;break;}}
        require(e.id==s.get_string("id")&&e.label==label&&e.owner==(s.get("source_parent_subgraph").is_null()?"":s.get_string("source_parent_subgraph"))&&e.shape=="box","source: ER107 identity/first-nonempty alias/owner");
        const auto hr=wrap(label,w);require(e.headers.size()==1&&e.headers[0].size()==hr.size(),"source: ER107 complete header rows");
        for(std::size_t j=0;j<hr.size();++j)require(e.headers[0][j]==hr[j].text,"source: ER107 header spelling");
        const auto attrs=s.get("attributes");require(e.rows.size()==attrs.size()&&e.typed_er.has_value()==(attrs.size()>0),"source: ER107 complete typed attribute rows");
        require(p.get("typed_attributes").size()==attrs.size(),"source: ER107 field fact population");
        for(std::size_t j=0;j<attrs.size();++j) {
            const auto a=attrs.at(j),pa=p.get("typed_attributes").at(j);const auto& field=e.typed_er->attributes[j];
            require(field.type==generic_display(a.get("type_literal").as_str(),w)&&field.name==a.get_string("name_literal")&&field.comment==a.get_string("comment_literal")&&field.keys.size()==a.get("declared_key_tokens").size(),"source: ER107 exact raw-generic/display/name/comment/key facts");
            for(std::size_t k=0;k<field.keys.size();++k)require(field.keys[k]==a.get("declared_key_tokens").at(k).as_str(),"source: ER107 key order/no invented PK");
            exact(pa.get("display_fields"),n.get("rows").at(j).get("source_fields"),w,"source: ER107 coherent typed field duplication");
            require(pa.get_string("raw_type")==a.get_string("type_literal"),"source: ER107 raw generic retained");
            const auto literal=n.get("typed_er").get("attributes").at(j);
            exact(literal,n.get("rows").at(j).get("source_fields"),w,"source: ER107 complete typed table duplication");
        }
        if(attrs.size())exact(n.get("typed_er").get("header"),n.get("headers").at(0),w,"source: ER107 typed table header");else require(n.get("typed_er").is_null(),"source: ER107 empty table null");
        EntityRole r;r.id=e.id;r.owner=e.owner;r.source_index=static_cast<int>(sd::integer(s.get("source_getData_colorIndex"),w));
        require(r.source_index==static_cast<int>(i),"source: ER107 all non-group insertion indices");
        if(out.categorical) {
            r.slot=r.source_index%12;
            auto palette=[&](std::string_view field)->JsonVal {
                for(std::size_t k=0;k<palettes.size();++k){const auto pl=palettes.at(k);if(pl.get_string("theme")==out.theme&&pl.get_string("source_field")==field)return pl.get("entries");}
                throw std::runtime_error("source: ER107 missing source palette");
            };
            const auto borders=palette("borderColorArray");require(borders.size()==12,"source: ER107 border palette length");r.border_hue=rgb_hue(borders.at(static_cast<std::size_t>(*r.slot)).get("source_literal").as_str(),w);r.foreground=hue_role(r.border_hue);
            const auto fills=palette("bkgColorArray");require(fills.size()==(out.theme=="redux-color"?12u:0u),"source: ER107 independent fill palette length");
            if(fills.size()){r.fill_hue=rgb_hue(fills.at(static_cast<std::size_t>(*r.slot)).get("source_literal").as_str(),w);r.fill=hue_role(r.fill_hue);}
        }
        require(p.get_string("id")==r.id&&p.get_int("source_color_index")==r.source_index&&p.get_int("border_role")==r.foreground&&p.get_int("fill_role")==r.fill&&p.get_string("border_hue")==r.border_hue&&p.get_string("fill_hue")==r.fill_hue&&!p.get("bold").as_bool(),"source: ER107 independent source-index/hue/role law");
        require(r.slot?p.get_int("categorical_slot")==*r.slot:p.get("categorical_slot").is_null(),"source: ER107 categorical slot ownership");
        require(p.get_string("border_background_token")== (r.owner.empty()?"surface_tint_2":"surface_tint_4")&&p.get_string("internal_rail_background")=="owning_entity_fill","source: ER107 outer/inner rail background distinction");
        require(p.get_int("source_stroke_width_px")==((out.look=="handDrawn"||out.theme=="neo-dark")?1:2),"source: ER1071px/2px normal source width");
        out.roles.push_back(std::move(r));
    }
    for(std::size_t i=0;i<edges.size();++i) {
        const auto s=edges.at(i),p=terminal.get("relationship_presentations").at(i);const auto& e=out.scene.relations[i];
        const auto display=role_display(s.get("role_literal").as_str(),w),physical=display.empty()?"":multiline(wrap(display,w));
        require(e.from==s.get_string("entityA")&&e.to==s.get_string("entityB")&&e.order==static_cast<int>(i)&&e.label==physical&&e.start==marker(s.get("source_relSpec").get("cardB").as_str())&&e.end==marker(s.get("source_relSpec").get("cardA").as_str())&&e.stroke==(s.get_string("source_getData_pattern")=="solid"?"solid":"dotted"),"source: ER107 complete cardinality/orientation/stroke/role");
        require(p.get_string("raw_role")==s.get_string("role_literal")&&p.get_string("display_role")==display&&p.get("blank_role").as_bool()==display.empty(),"source: ER107 distinct raw/display blank role");
    }
    for(std::size_t i=0;i<groups.size();++i) {
        const auto g=groups.at(i);const auto& v=out.scene.groups[i];
        require(v.id==g.get_string("id")&&v.label==g.get_string("source_title_before_sanitize")&&v.parent.empty()&&v.direction.empty()&&v.members.size()==g.get("members").size(),"source: ER107 safe ASCII group identity");
        for(std::size_t j=0;j<v.members.size();++j)require(v.members[j]==g.get("members").at(j).as_str(),"source: ER107 complete group membership");
    }
    // Exact finite caption facts are independently re-derived from B1; these
    // literals describe the frozen fixtures and are not a production allowlist.
    sd::pay(w,encoded.size(),4,1024);
    std::map<std::string,std::pair<std::string,std::string>> source_captions;
    for(std::size_t i=0;i<nodes.size();++i) {
        const auto n=nodes.at(i);const auto id=n.get_string("id");auto label=n.get_string("source_db_label");
        const auto attempts=n.get("alias_attempts");for(std::size_t j=0;j<attempts.size();++j){const auto v=attempts.at(j).get_string("value");if(!v.empty()){label=v;break;}}
        source_captions.emplace("entity/"+id+"/header",std::pair{label,label});
        const auto attrs=n.get("attributes");for(std::size_t j=0;j<attrs.size();++j){const auto at=attrs.at(j);const auto stem="entity/"+id+"/attribute/"+std::to_string(j)+"/";const auto raw=at.get_string("type_literal");
            source_captions.emplace(stem+"type",std::pair{raw,generic_display(raw,w)});
            const auto name=at.get_string("name_literal"),comment=at.get_string("comment_literal");source_captions.emplace(stem+"name",std::pair{name,name});source_captions.emplace(stem+"comment",std::pair{comment,comment});
            std::string keys;const auto tokens=at.get("declared_key_tokens");for(std::size_t k=0;k<tokens.size();++k){if(k)keys+=',';keys+=tokens.at(k).as_str();}source_captions.emplace(stem+"keys",std::pair{keys,keys});
        }
    }
    for(std::size_t i=0;i<groups.size();++i){const auto g=groups.at(i);const auto value=g.get_string("source_title_before_sanitize");source_captions.emplace("group/"+g.get_string("id")+"/header",std::pair{value,value});}
    for(std::size_t i=0;i<edges.size();++i){const auto raw=edges.at(i).get_string("role_literal");source_captions.emplace("edge/"+std::to_string(i)+"/role",std::pair{raw,role_display(raw,w)});}
    require(source_captions.size()==terminal.get("captions").size(),"source: ER107 complete caption domain");
    std::set<std::string> caption_ids;
    for(std::size_t i=0;i<terminal.get("captions").size();++i){const auto c=terminal.get("captions").at(i);const auto id=c.get_string("handle");const auto found=source_captions.find(id);require(found!=source_captions.end()&&caption_ids.insert(id).second&&c.get_string("raw")==found->second.first&&c.get_string("display")==found->second.second,"source: ER107 exact raw/display caption ownership");}
    // Verify every display-row/joiner record independently of the immutable pin.
    const auto captions=terminal.get("captions");require(captions.size()<=4096,"resource: ER107 caption fact limit");
    for(std::size_t i=0;i<captions.size();++i){const auto cap=captions.at(i);checked_rows(cap.get("rows"),cap.get("display").as_str(),w);}
    auto add=[&](std::string v){sd::pay(w,v.size(),8,128);out.notice_rows.push_back(wrap(v,w));out.notices.push_back(std::move(v));};
    add("ER native projection v1");const auto leaves=meta.get("ordered_config_leaves");
    for(std::size_t i=0;i<leaves.size();++i){const auto l=leaves.at(i),path=l.get("path");std::string name;for(std::size_t j=0;j<path.size();++j){if(j)name+='.';name+=path.at(j).as_str();}add(name+"="+canonical(l.get("source_value"),w));}
    add("effective theme="+out.theme+"; look="+out.look);
    add("look="+out.look+" -> native orthogonal; roughness/relief not reproduced");
    if(cfg.has("layout"))add("layout=elk -> native directed routing");
    if(cfg.has("htmlLabels"))add("htmlLabels=false -> native Markdown subset");
    if(cfg.has("er"))add("er.useMaxWidth=false -> SVG viewport only; native width fallback unchanged");
    add("theme RGB/font/CSS -> terminal roles; foreign assertions pending");
    for(const auto& r:out.roles)if(r.slot)add("entity "+r.id+" slot="+std::to_string(*r.slot)+" border="+r.border_hue+" fill="+r.fill_hue);
    for(std::size_t i=0;i<edges.size();++i){const auto r=edges.at(i);if(role_display(r.get("role_literal").as_str(),w).empty())add("edge "+std::to_string(i)+" raw-role="+canonical(r.get("role_literal"),w)+" -> no visible words");}
    const auto notices=terminal.get("notices");require(notices.size()==out.notices.size(),"source: ER107 full ordered notice catalog");
    for(std::size_t i=0;i<out.notices.size();++i){const auto n=notices.at(i);require(n.get_int("ordinal")==static_cast<std::int64_t>(i)&&n.get_string("text")==out.notices[i],"source: ER107 exact authored/default/slot/debt/blank notice");checked_rows(n.get("rows"),out.notices[i],w);require(n.get("style").get_string("foreground_token")=="text"&&n.get("style").get_string("background")=="inherited"&&!n.get("style").get("bold").as_bool(),"source: ER107 notice full style");}
    return out;
}
struct BoundCase {
    source::BoundFacts source;
    JsonVal terminal_case,terminal_model;
    Projection projection;
};
// No deep-root constructor is required. A B3 adapter may call this only AFTER
// its own complete raw-source public captures. Final global validation follows
// all captures. It must not use source::verify_roots as a pre-capture shortcut.
inline BoundCase bind_case(JsonVal source_root,JsonVal terminal_root,JsonVal manifest_root,JsonVal prior_root,
    std::string_view path,std::string_view raw,OracleWork& w) {
    require(raw.size()<=source::kMaxRawBytes,"resource: ER107 complete raw64KiB");
    const auto it=std::ranges::find_if(kPins,[&](const Pin& p){w.spend(32);return p.path==path;});
    require(it!=kPins.end(),"source: ER107 projection path outside cohort");const auto i=static_cast<std::size_t>(it-kPins.begin());const auto& p=source::kPins[i];
    require(source_root.get("cases").size()==107&&terminal_root.get("cases").size()==107,"source: ER107 root case counts");
    const auto s=source_root.get("cases").at(i),t=terminal_root.get("cases").at(i),record=manifest_root.get("fixtures").at(static_cast<std::size_t>(p.global)),old=prior_root.get("cases").at(static_cast<std::size_t>(p.prior_ordinal));
    sd::pin(s,p.case_sha,w);sd::pin(record,p.manifest_sha,w);sd::pin(old,p.prior_case_sha,w);
    require(raw.size()==p.raw_bytes&&sd::digest(raw,w)==p.raw_sha&&s.get("full_source").as_str()==raw&&old.get("source").as_str()==raw,"source: ER107 complete original/manifest/prior binding");
    // Git identity is immutable in both independently approved manifest/prior
    // canonical pins; direct SHA1 is not introduced by this pure helper.
    require(record.get_string("git_blob")==p.git_blob&&s.get_string("source_git_blob")==p.git_blob,"source: ER107 frozen Git identity");
    source::detail::check_unprojected(s,w);source::detail::witness_slices(s,raw,w);
    sd::pin(t,it->case_sha,w,"source: ER107 immutable projection case pin");
    const auto tm=t.get("terminal_model"),binding=t.get("source_binding"),shared=terminal_root.get("shared_contract"),palette=terminal_root.get("palette_source_and_projection");
    sd::pin(shared,kSharedSha,w);sd::pin(palette,kPaletteSha,w);
    sd::pin(terminal_root.get("contract_basis"),kContractBasisSha,w);
    sd::pin(tm,it->model_sha,w);sd::pin(tm.get("whole_scene_model"),it->scene_sha,w);sd::pin(binding,it->binding_sha,w);
    require(t.get_string("path")==path&&t.get_int("manifest_global_ordinal")==p.global&&t.get_int("ordinal")==static_cast<std::int64_t>(i),"source: ER107 exact ordered identity/global");
    require(binding.get_string("source_model_sha256")==p.model_sha&&binding.get_string("source_witnesses_sha256")==p.witness_sha&&binding.get_string("manifest_record_sha256")==p.manifest_sha&&binding.get_string("source_sha256")==p.raw_sha,"source: ER107 cross-asset component joins");
    const auto joined="{\"shared_contract\":"+canonical(shared,w)+",\"source_binding\":"+canonical(binding,w)+",\"terminal_model\":"+canonical(tm,w)+"}";
    require(joined.size()<=source::kMaxFactBytes&&sd::digest(joined,w)==it->complete_sha,"source: ER107 full complete-model preimage pin");
    require(t.get_string("complete_model_sha256")==it->complete_sha&&t.get_string("terminal_model_sha256")==it->model_sha&&t.get_string("whole_scene_model_sha256")==it->scene_sha&&t.get_string("source_binding_sha256")==it->binding_sha,"source: ER107 declared hash pin");
    source::BoundFacts facts{s,s.get("source_model"),s.get("source_witnesses"),record,old,std::string(path),p.global};
    return {facts,t,tm,decode_semantics(tm,facts.source_model,palette,w)};
}
// Complete full-array/object preimages, never arrays of selected digest fields.
inline std::string component_preimage(JsonVal root,std::string_view key,OracleWork& w) {
    sd::pay(w,source::kMaxFactBytes,1,128);std::string out;out.reserve(source::kMaxFactBytes);
    auto add=[&](std::string_view s){require(s.size()<=source::kMaxFactBytes-out.size(),"resource: ER107 full component4MiB");sd::pay(w,s.size(),1);out+=s;};
    const auto cases=root.get("cases");require(cases.size()==107,"source: ER107 full component population");
    const bool full=key=="complete_models_sha256";
    if(full){add("{\"cases\":");}
    add("[");
    for(std::size_t i=0;i<cases.size();++i) {
        if(i)add(",");const auto c=cases.at(i);add("{\"path\":");add(canonical(c.get("path"),w));
        if(full||key=="source_bindings_sha256"){add(",\"source_binding\":");add(canonical(c.get("source_binding"),w));}
        if(full||key=="terminal_models_sha256"){add(",\"terminal_model\":");add(canonical(c.get("terminal_model"),w));}
        if(key=="whole_scene_models_sha256"){add(",\"whole_scene_model\":");add(canonical(c.get("terminal_model").get("whole_scene_model"),w));}add("}");
    }
    add("]");if(full){add(",\"shared_contract\":");add(canonical(root.get("shared_contract"),w));add("}");}return out;
}
// Root validation has two explicit independent caller-owned stages, each capped
// at512M test-oracle units: B1 source roots, then full projection components.
// Aggregate root-stage maximum is1,024M; this is neither production1M Work nor
// a reset/refund of a live counter. All per-case and full-preimage checks remain.
inline constexpr std::size_t kRootPhaseWorkCap=512'000'000;
inline void validate_roots(JsonVal source_root,JsonVal terminal_root,JsonVal presentation_root,JsonVal manifest_root,JsonVal prior_root,
    OracleWork& source_root_work,OracleWork& projection_component_work) {
    require(&source_root_work!=&projection_component_work,"protocol: ER107 independent root-stage ledgers required");
    require(source_root_work.left<=kRootPhaseWorkCap&&projection_component_work.left<=kRootPhaseWorkCap,"resource: ER107 root-stage512M cap");
    static_cast<void>(source::verify_roots(source_root,manifest_root,prior_root,source_root_work));
    auto& w=projection_component_work;
    sd::pin(terminal_root,kProjectionSha,w,"source: ER107 full immutable projection root");sd::pin(presentation_root,kPresentationCanonicalSha,w);
    require(terminal_root.get_string("review_status")==kReviewStatus&&terminal_root.get_int("drawing_credit")==0&&!terminal_root.get("formal_admission").as_bool(),"source: ER107 UNREVIEWED zero admission");
    const auto cases=terminal_root.get("cases");require(cases.size()==kPins.size(),"source: ER107 all107 projection identities");
    for(std::size_t i=0;i<kPins.size();++i){const auto c=cases.at(i);require(c.get_string("path")==kPins[i].path&&c.get_int("ordinal")==static_cast<std::int64_t>(i)&&c.get_int("manifest_global_ordinal")==kPins[i].global,"source: ER107 all ordered case pins");sd::pin(c,kPins[i].case_sha,w);}
    for(const auto& [name,expected]:kComponents) {
        sd::same_text(terminal_root.get("component_hashes").get(name),expected,w,"source: ER107 root component pin");
        if(name=="palette_source_and_projection_sha256")sd::pin(terminal_root.get("palette_source_and_projection"),expected,w);
        else if(name=="upstream_evidence_references_sha256")sd::pin(terminal_root.get("upstream_evidence_references"),expected,w);
        else if(name=="additional_upstream_evidence_sha256")sd::pin(terminal_root.get("additional_upstream_evidence"),expected,w);
        else require(sd::digest(component_preimage(terminal_root,name,w),w)==expected,"source: ER107 complete root component preimage");
    }
}
inline void validate_contract_excerpts(JsonVal root,std::string_view current_document,OracleWork& w) {
    require(current_document.size()<=262144,"resource: ER107 contract bound");
    sd::pin(root.get("contract_basis"),kContractBasisSha,w);
    const auto segments=root.get("contract_basis").get("segments");
    require(segments.size()==5,"source: ER107 complete five contract sections");
    for(std::size_t i=0;i<segments.size();++i){const auto s=segments.at(i);const auto exact=s.get("exact_excerpt").as_str();require(sd::digest(exact,w)==s.get("slice_sha256").as_str(),"source: ER107 contract slice hash");sd::pay(w,current_document.size(),2);require(current_document.find(exact)!=std::string_view::npos,"source: ER107 bound substantive contract excerpt changed");}
    // The whole-document SHA remains historical provenance. Additional review
    // status text may be appended without creating an asset/document hash cycle.
}

// Caller owns actual concrete color strings (the established Block authority
// convention), not role guesses. It must populate these from its live palette
// and captured cells. The following fixtures use invented distinguishable
// strings only to exercise this pure comparison protocol, not application RGB.
struct Surfaces {
    std::array<std::string,7> foreground,background;
    std::string inherited_background;
    std::uint16_t inherited_dim=0;
    // Independent caller wrapper context; no capture-derived style inference.
    std::uint16_t block_exterior_dim=0;
};
struct ObservedCell {
    std::string glyph=" ",foreground,background;
    bool bold=false;std::uint16_t dim=0,other_attributes=0;std::uint8_t hyperlink=0;
    bool operator==(const ObservedCell&) const=default;
};
struct Surface {std::vector<std::vector<ObservedCell>> rows;};
struct CellRole {int foreground=1,background=1;bool inherited_background=false;};
inline void check_cell(const ObservedCell& got,std::string_view glyph,CellRole role,const Surfaces& surfaces,OracleWork& w) {
    require(role.foreground>=1&&role.foreground<=6&&role.background>=1&&role.background<=6,"protocol: ER107 finite cell roles");
    sd::pay(w,got.glyph.size()+got.foreground.size()+got.background.size(),2,32);
    const auto& bg=role.inherited_background?surfaces.inherited_background:surfaces.background[static_cast<std::size_t>(role.background)];
    require(got.glyph==glyph&&got.foreground==surfaces.foreground[static_cast<std::size_t>(role.foreground)]&&got.background==bg&&!got.bold&&got.dim==surfaces.inherited_dim&&got.other_attributes==0&&got.hyperlink==0,"physical: ER107 exact glyph/FG/BG/bold/dim/attributes");
}
inline void surface_limits(const Surface& observed,OracleWork& w) {
    require(!observed.rows.empty()&&observed.rows.size()<=1024,"resource: ER107 surface1024 rows");
    std::size_t cells=0;
    for(const auto& row:observed.rows){require(row.size()<=240,"resource: ER107 surface240 columns");cells+=row.size();for(const auto& cell:row)require(cell.glyph.size()<=16&&cell.foreground.size()<=128&&cell.background.size()<=128,"resource: ER107 bounded actual cell");}
    sd::pay(w,cells,32,128);
}
inline bool same_rectangle(const relation::Rect& a,const relation::Rect& b) {
    return a.l==b.l&&a.r==b.r&&a.t==b.t&&a.b==b.b&&a.shape==b.shape;
}
inline std::string grid_binding(const relation::Grid& g,OracleWork& w) {
    require(g.width>0&&g.width<=240&&g.height>0&&g.height<=1024&&g.cells.size()==static_cast<std::size_t>(g.width*g.height),"resource: ER107 contact grid bounds");
    sd::pay(w,g.cells.size(),18,128);std::string bytes=std::to_string(g.width)+","+std::to_string(g.height)+";";
    for(const auto& v:g.cells){require(v.size()<=16,"resource: ER107 contact glyph bound");bytes+=v;bytes+='\0';}
    return sd::digest(bytes,w);
}
inline std::string model_binding(const relation::Model& m,OracleWork& w) {
    require(m.entities.size()<=64&&m.relations.size()<=128&&m.groups.size()<=64&&m.notes.size()<=64,"resource: ER107 contact Model bound");
    sd::pay(w,65536,2,128);std::string bytes;bytes.reserve(65536);
    auto put=[&](std::string_view v){require(v.size()<=65536&&bytes.size()+v.size()+32<=source::kMaxFactBytes,"resource: ER107 Model fingerprint bytes");sd::pay(w,v.size(),4,64);bytes+=std::to_string(v.size());bytes+=':';bytes+=v;};
    auto number=[&](std::size_t n){put(std::to_string(n));};put(m.family);put(m.direction);number(m.entities.size());
    for(const auto& e:m.entities){put(e.id);put(e.label);put(e.shape);put(e.owner);number(e.headers.size());for(const auto& h:e.headers){number(h.size());for(const auto& v:h)put(v);}number(e.rows.size());for(const auto& r:e.rows){put(r.role);number(r.alternatives.size());for(const auto& v:r.alternatives)put(v);}put(e.typed_er?"typed":"untyped");if(e.typed_er){number(e.typed_er->header.size());for(const auto& h:e.typed_er->header)put(h);number(e.typed_er->attributes.size());for(const auto& f:e.typed_er->attributes){put(f.type);put(f.name);number(f.keys.size());for(const auto& k:f.keys)put(k);put(f.comment);}}}
    number(m.relations.size());for(const auto& r:m.relations){put(r.from);put(r.to);put(r.label);put(r.start);put(r.end);put(r.stroke);put(std::to_string(r.order));put(r.source_text?"present":"absent");if(r.source_text)put(*r.source_text);put(r.target_text?"present":"absent");if(r.target_text)put(*r.target_text);}
    number(m.groups.size());for(const auto& g:m.groups){put(g.id);put(g.label);put(g.parent);put(g.direction);number(g.members.size());for(const auto& v:g.members)put(v);}
    number(m.notes.size());for(const auto& n:m.notes){put(n.text);put(n.target);put(n.position);put(n.owner);}return sd::digest(bytes,w);
}
struct LabelOwner {std::size_t relation_index=0;relation::Rect content;};
class InlineContacts;
InlineContacts adapt_label_contacts(const relation::Grid&,const relation::Model&,const relation::ErLabelContactsWitness&,
    const std::vector<relation::Rect>&,const std::vector<relation::Rect>&,LabelOwner,OracleWork&);
// The Main-owned oracle is the only issuer. This adapter never calls raw
// check_scene, reconstructs a selected trace, or guesses a contact from glyphs.
// It borrows the noncopyable receipt and verifies its current validity/tags,
// exact relationship index/order and matched gap, plus immutable content hashes.
class InlineContacts {
    const relation::Grid* grid_=nullptr;
    const relation::Model* model_=nullptr;
    const relation::ErLabelContactsWitness* issued_=nullptr;
    std::string grid_hash_,model_hash_;
    std::vector<relation::Rect> entities_,groups_;
    relation::ErLabelBridge bridge_;
    relation::Rect label_;
    std::array<relation::Point,2> points_;
    InlineContacts(const relation::Grid& g,const relation::Model& m,const relation::ErLabelContactsWitness& issued,
        std::string gh,std::string mh,std::vector<relation::Rect> es,std::vector<relation::Rect> gs,
        relation::ErLabelBridge bridge,relation::Rect label)
        :grid_(&g),model_(&m),issued_(&issued),grid_hash_(std::move(gh)),model_hash_(std::move(mh)),
         entities_(std::move(es)),groups_(std::move(gs)),bridge_(std::move(bridge)),label_(std::move(label)),points_{{bridge_.left,bridge_.right}}{}
    friend InlineContacts adapt_label_contacts(const relation::Grid&,const relation::Model&,const relation::ErLabelContactsWitness&,
        const std::vector<relation::Rect>&,const std::vector<relation::Rect>&,LabelOwner,OracleWork&);
public:
    const auto& points()const{return points_;}
    const auto& label()const{return label_;}
    std::size_t relation_index()const{return bridge_.relation_index;}
    void verify(const relation::Grid& g,const std::vector<relation::Rect>& es,
        const std::vector<relation::Rect>& gs,const std::vector<relation::Rect>& labels,OracleWork& w)const {
        require(issued_&&issued_->valid()&&issued_->grid()==grid_&&issued_->model()==model_,"physical: ER107 cleared or foreign shared contact witness");
        require(grid_==&g&&grid_hash_==grid_binding(g,w)&&model_hash_==model_binding(*model_,w),"physical: ER107 stale or foreign contact grid/Model");
        bool still_selected=false;for(const auto& r:issued_->bridges()){w.spend(32);if(r.relation_index==bridge_.relation_index&&r.relation_order==bridge_.relation_order&&same_rectangle(r.label_box,bridge_.label_box)&&r.left==bridge_.left&&r.right==bridge_.right)still_selected=true;}
        require(still_selected,"physical: ER107 contact no longer selected by shared oracle");
        require(es.size()==entities_.size()&&gs.size()==groups_.size(),"physical: ER107 contact outline population");
        for(std::size_t i=0;i<es.size();++i)require(same_rectangle(es[i],entities_[i]),"physical: ER107 contact entity assignment");
        for(std::size_t i=0;i<gs.size();++i)require(same_rectangle(gs[i],groups_[i]),"physical: ER107 contact group assignment");
        require(std::ranges::count_if(labels,[&](const auto& b){return same_rectangle(b,label_);})==1,"physical: ER107 contact label assignment");
    }
};
inline InlineContacts adapt_label_contacts(const relation::Grid& g,const relation::Model& m,
    const relation::ErLabelContactsWitness& issued,const std::vector<relation::Rect>& es,
    const std::vector<relation::Rect>& gs,LabelOwner owner,OracleWork& w) {
    require(issued.valid()&&issued.grid()==&g&&issued.model()==&m&&m.family=="er","physical: ER107 valid same-grid/source-Model contact receipt required");
    require(owner.relation_index<m.relations.size()&&m.relations.size()<=128,"physical: ER107 contact source relationship index");
    require(es.size()==m.entities.size()&&gs.size()==m.groups.size(),"physical: ER107 contact outline population");
    const relation::ErLabelBridge* selected=nullptr;
    for(const auto& bridge:issued.bridges()) {
        w.spend(32);const auto& b=bridge.label_box;
        if(bridge.relation_index!=owner.relation_index||bridge.relation_order!=m.relations[owner.relation_index].order||b.l+1!=owner.content.l||b.r-1!=owner.content.r||b.t+1!=owner.content.t||b.b-1!=owner.content.b)continue;
        require(!selected,"physical: ER107 ambiguous same-relationship bridge");selected=&bridge;
    }
    require(selected,"physical: ER107 no selected same-relationship label bridge");
    for(const auto q:{selected->left,selected->right}) {
        require(g.has(q),"physical: ER107 certified contact outside grid");
        for(const auto& e:es)require(!e.interior(q)&&!e.boundary(q),"physical: ER107 contact overwrites entity");
        for(const auto& b:gs)require(!b.boundary(q),"physical: ER107 contact overwrites group gate");
    }
    sd::pay(w,es.size()+gs.size(),2*sizeof(relation::Rect)+128,512);
    return InlineContacts(g,m,issued,grid_binding(g,w),model_binding(m,w),es,gs,*selected,owner.content);
}
inline std::vector<CellRole> primitive_roles(const relation::Grid& grid,const std::vector<relation::Rect>& entities,
    const std::vector<relation::Rect>& groups,const std::vector<EntityRole>& roles,
    const std::vector<relation::Rect>& label_content,OracleWork& w,std::span<const InlineContacts> contacts={}) {
    require(grid.width>0&&grid.width<=240&&grid.height>0&&grid.height<=1024&&grid.cells.size()==static_cast<std::size_t>(grid.width*grid.height),"resource: ER107 primitive grid bound");
    require(entities.size()==roles.size()&&entities.size()<=64&&groups.size()<=64&&label_content.size()<=128,"protocol: ER107 primitive ownership counts");
    sd::pay(w,grid.cells.size(),sizeof(CellRole)+32,128);std::vector<CellRole> result(grid.cells.size());
    require(contacts.size()<=128,"resource: ER107 contact witness count");
    sd::pay(w,grid.cells.size(),1,64);std::vector<bool> committed_contacts(grid.cells.size(),false);
    for(const auto& witness:contacts){witness.verify(grid,entities,groups,label_content,w);for(const auto p:witness.points()){const auto index=static_cast<std::size_t>(grid.index(p));require(!committed_contacts[index],"physical: ER107 shared contact owner");committed_contacts[index]=true;}}
    auto contains=[](const relation::Rect& box,relation::Point p){return box.interior(p)||box.boundary(p);};
    auto bounded=[&](const relation::Rect& b){require(b.l>=0&&b.t>=0&&b.r<grid.width&&b.b<grid.height&&b.l<b.r&&b.t<b.b,"protocol: ER107 primitive rectangle bound");};
    for(const auto& b:groups)bounded(b);for(const auto& b:entities)bounded(b);
    for(int y=0;y<grid.height;++y)for(int x=0;x<grid.width;++x) {
        w.spend(16);const relation::Point p{x,y};auto& cell=result[static_cast<std::size_t>(grid.index(p))];
        for(const auto& g:groups){w.spend(4);if(g.interior(p))cell={1,6,false};else if(g.boundary(p))cell={1,1,false};}
        for(std::size_t i=0;i<entities.size();++i) {
            w.spend(8);const auto& b=entities[i];if(!contains(b,p))continue;const auto& r=roles[i];
            if(b.boundary(p))cell={r.foreground,r.owner.empty()?1:6,false};
            else cell={relation::glyph(grid.at(p)).bits?r.foreground:1,r.fill,false};
        }
        for(const auto& b:label_content) {
            w.spend(8);if(y<b.t||y>b.b||x<b.l-1||x>b.r+1)continue;
            for(const auto& e:entities)require(!contains(e,p),"physical: ER107 label/clearance overwrites entity");
            for(const auto& g:groups)require(!g.boundary(p),"physical: ER107 label/clearance overwrites group gate");
            if((x==b.l-1||x==b.r+1)&&grid.at(p)!=" ") {
                bool owned=false;
                for(const auto& witness:contacts){w.spend(8);if(same_rectangle(witness.label(),b)&&std::ranges::find(witness.points(),p)!=witness.points().end())owned=true;}
                require(committed_contacts[static_cast<std::size_t>(grid.index(p))]&&owned,"physical: ER107 nonblank label side cell lacks committed contact proof");
                continue; // Preserve the complete host shaft FG/BG/bold tuple.
            }
            cell={1,1,false}; // Caption and OWNED U+0020 side clearance use root BG.
        }
    }
    return result;
}
// This validates a complete independently authored primitive, NOT a caller-
// asserted complete-scene geometry witness. B3 must recover and validate all
// rectangles/routes/labels independently before using the same style laws.
inline void validate_primitive(const relation::Grid& expected_glyphs,const Surface& observed,
    const std::vector<relation::Rect>& entities,const std::vector<relation::Rect>& groups,
    const std::vector<EntityRole>& roles,const std::vector<relation::Rect>& label_content,
    const Surfaces& surfaces,OracleWork& w,std::span<const InlineContacts> contacts={}) {
    surface_limits(observed,w);require(observed.rows.size()==static_cast<std::size_t>(expected_glyphs.height),"physical: ER107 full primitive height");
    const auto expected_roles=primitive_roles(expected_glyphs,entities,groups,roles,label_content,w,contacts);
    for(int y=0;y<expected_glyphs.height;++y){require(observed.rows[static_cast<std::size_t>(y)].size()==static_cast<std::size_t>(expected_glyphs.width),"physical: ER107 full primitive width");for(int x=0;x<expected_glyphs.width;++x){const auto i=static_cast<std::size_t>(expected_glyphs.index({x,y}));check_cell(observed.rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)],expected_glyphs.at({x,y}),expected_roles[i],surfaces,w);}}
}
inline void validate_notice_surface(const Projection& projection,const Surface& observed,const Surfaces& surfaces,OracleWork& w) {
    surface_limits(observed,w);std::size_t count=0;for(const auto& rows:projection.notice_rows)count+=rows.size();
    require(observed.rows.size()==count,"physical: ER107 complete notice height");std::size_t y=0;
    for(const auto& rows:projection.notice_rows)for(const auto& row:rows){const auto& actual=observed.rows[y++];require(actual.size()==row.text.size(),"physical: ER107 exact notice width/no padding");for(std::size_t x=0;x<row.text.size();++x)check_cell(actual[x],std::string_view(row.text).substr(x,1),{1,1,true},surfaces,w);}
}
// Finite source-only worksheet. Natural rectangles are lower bounds on each
// primitive, not placements or a claim that the complete scene has a solution.
struct PrimitiveBound {std::string id;std::size_t width=0,height=0,unicode_cells_min=0;};
struct SourceWorksheet {
    std::vector<PrimitiveBound> primitives;
    std::size_t notice_A=0,notice_B=0,notice_H=0;
    std::size_t entity_area_min=0,body_width_min=0,body_height_min=0,body_A_min=0,body_unicode_min=0;
    std::size_t working_M_cap=240u*1024u;
    std::optional<std::size_t> complete_M,complete_A,complete_B,complete_R,complete_H;
    bool complete_scene_plan_proven=false,production_new_storage_known=false;
};
inline SourceWorksheet source_worksheet(const Projection& p,OracleWork& w) {
    SourceWorksheet out;sd::pay(w,p.scene.entities.size(),sizeof(PrimitiveBound)+128,128);
    for(const auto& e:p.scene.entities) {
        w.spend(128);PrimitiveBound b;b.id=e.id;std::size_t header_width=0,header_rows=0;
        require(e.headers.size()==1,"source: ER107 worksheet single header block");
        for(const auto& row:e.headers[0]){sd::pay(w,row.size(),2,8);header_width=std::max(header_width,row.size());++header_rows;}
        b.width=std::max(std::size_t{3},header_width+2);b.height=header_rows+2;
        if(e.typed_er) {
            const auto& as=e.typed_er->attributes;const bool keys=std::ranges::any_of(as,[](const auto& a){return !a.keys.empty();}),comments=std::ranges::any_of(as,[](const auto& a){return !a.comment.empty();});
            std::array<std::size_t,4> widths{1,1,1,1};std::size_t content_rows=0;
            for(const auto& a:as){w.spend(64);const std::array<std::string,4> fields{a.type,a.name,relation::er_key_text(a.keys),a.comment};std::size_t height=1;
                for(std::size_t k=0;k<fields.size();++k){if((k==2&&!keys)||(k==3&&!comments))continue;const auto rs=wrap(fields[k],w);height=std::max(height,rs.size());for(const auto& r:rs)widths[k]=std::max(widths[k],r.text.size());}content_rows+=height;
            }
            const std::size_t columns=2u+(keys?1u:0u)+(comments?1u:0u);std::size_t table_width=1;
            for(std::size_t k=0;k<widths.size();++k)if((k!=2||keys)&&(k!=3||comments))table_width+=widths[k]+3;
            b.width=std::max(b.width,table_width);b.height=header_rows+2+content_rows+as.size();
            b.unicode_cells_min=2*b.width+2*b.height-4+as.size()*(b.width-2)+(columns-1)*content_rows;
        }else b.unicode_cells_min=2*b.width+2*b.height-4;
        require(b.width<=240&&b.height<=1024,"source: ER107 primitive minimum exceeds canvas");
        out.entity_area_min+=b.width*b.height;out.body_unicode_min+=b.unicode_cells_min;
        out.body_width_min=std::max(out.body_width_min,b.width+2);out.body_height_min=std::max(out.body_height_min,b.height+2);out.primitives.push_back(std::move(b));
    }
    out.body_A_min=std::max(out.entity_area_min,out.body_width_min*out.body_height_min);
    for(const auto& record:p.notice_rows)for(const auto& r:record){sd::pay(w,r.text.size(),1,8);out.notice_A+=r.text.size();out.notice_B+=r.text.size();++out.notice_H;}
    // All source display is ASCII; body box/stroke glyphs are three-byte UTF-8.
    // B_body = A_body + 2*U_body, U_body >= the primitive Unicode lower bound.
    // H_total = H_body + 1 empty separator + notice_H; A excludes that empty row.
    // No lower bound sets complete_* or production_new_storage_known.
    return out;
}
struct PlanCounts {std::size_t M=0,A=0,B=0,R=0,H=0;};
inline PlanCounts measure_primitive(const relation::Grid& grid,const Surface& observed,
    const std::vector<relation::Rect>& entities,const std::vector<relation::Rect>& groups,
    const std::vector<EntityRole>& roles,const std::vector<relation::Rect>& label_content,OracleWork& w) {
    surface_limits(observed,w);const auto semantic=primitive_roles(grid,entities,groups,roles,label_content,w);
    require(observed.rows.size()==static_cast<std::size_t>(grid.height),"protocol: ER107 worksheet primitive height");
    PlanCounts out;out.M=grid.cells.size();out.H=observed.rows.size();
    for(std::size_t y=0;y<observed.rows.size();++y){require(observed.rows[y].size()==static_cast<std::size_t>(grid.width),"protocol: ER107 worksheet primitive width");for(std::size_t x=0;x<observed.rows[y].size();++x){w.spend(16);++out.A;out.B+=observed.rows[y][x].glyph.size();const auto index=y*static_cast<std::size_t>(grid.width)+x;const auto r=semantic[index];if(x==0||r.foreground!=semantic[index-1].foreground||r.background!=semantic[index-1].background||r.inherited_background!=semantic[index-1].inherited_background)++out.R;}}
    return out;
}
struct PlanFees {
    std::size_t working_plane=0,crop_scan=0,output_scan=0,serialized_planes=0,utf8=0,runs=0,rows=0,total_known=0;
    std::size_t string_size=sizeof(std::string),element_size=sizeof(ftxui::Element);
    bool includes_extra_state_capacity_or_writes=false;
};
inline PlanFees minimum_plan_fees(const PlanCounts& n,OracleWork& w) {
    w.spend(256);require(n.M<=240u*1024u&&n.A<=240u*1024u&&n.B<=3u*240u*1024u&&n.R<=n.A&&n.H<=1024,"resource: ER107 worksheet finite plan counts");
    PlanFees f;f.working_plane=sizeof(std::uint16_t)*n.M;f.crop_scan=8*n.M;f.output_scan=8*n.A;f.serialized_planes=2*sizeof(std::uint16_t)*n.A;f.utf8=48*n.B;f.runs=(512+8*sizeof(std::string))*n.R;f.rows=(256+4*sizeof(ftxui::Element))*n.H;
    f.total_known=f.working_plane+f.crop_scan+f.output_scan+f.serialized_planes+f.utf8+f.runs+f.rows;return f;
}
// Extra production state, capacities, failed candidate visits, label contexts,
// generic scratch and every extra ownership/route write are ADDITIVE and not
// known from these source facts. They must never be silently recorded as zero.
// No localReady/admission API. Shared successful contact ownership is explicit;
// all107 complete-scene feasibility, complete style surfaces and public capture
// extraction still require separate independently reviewed B3 work.

// B3 checking consumes actual public frames as observations, never as source
// expectations. Contours/label gaps are candidates until the complete frozen
// Model and qualified old oracle accept their exact glyph ownership.
inline constexpr std::string_view kAdaptation="er107_complete_source_surface/v1";
inline constexpr std::string_view kB3ReviewStatus="UNREVIEWED";
inline constexpr std::string_view kB3Limitations=
    "Source-derived ASCII ER profile only; no foreign browser/RGB/font certification. "
    "Unique source header/table contour assignment is mandatory; ambiguity fails. "
    "Geometry is checked by the qualified whole-scene oracle and source crop laws, not coordinate goldens. "
    "This checker does not measure production1M Work, construct a planner, or grant admission.";
struct Frame {
    int width=0,height=0;
    std::vector<ObservedCell> cells;
    const ObservedCell& at(int x,int y)const{return cells.at(static_cast<std::size_t>(y*width+x));}
    ObservedCell& at(int x,int y){return cells.at(static_cast<std::size_t>(y*width+x));}
    bool operator==(const Frame&)const=default;
};
struct SceneEvidence {
    relation::Grid observed_body;
    relation::ErLabelContactsWitness contacts;
    std::vector<relation::Rect> entities,groups,labels;
    std::vector<InlineContacts> adapted_contacts;
    int body_width=0,body_height=0,total_rows=0,intrinsic_width=0;
    std::size_t source_relationships=0;
};
inline void frame_limits(const Frame& f,OracleWork& w) {
    require(f.width>0&&f.width<=240&&f.height>0&&f.height<=1040&&f.cells.size()==static_cast<std::size_t>(f.width*f.height),"resource: ER107 complete capture dimensions");
    sd::pay(w,f.cells.size(),8,128);
    for(const auto& c:f.cells){w.spend(2);require(!c.glyph.empty()&&c.glyph.size()<=16&&c.foreground.size()<=128&&c.background.size()<=128,"resource: ER107 complete captured cell bounds");}
}
inline int following_sentinel(const Frame& f,std::string_view token,OracleWork& w) {
    require(!token.empty()&&token.size()<=static_cast<std::size_t>(f.width),"protocol: ER107 source sentinel width");
    std::optional<int> row;
    for(int y=0;y<f.height;++y){w.spend(8);bool same=true;
        for(std::size_t x=0;x<token.size();++x){w.spend(1);if(f.at(static_cast<int>(x),y).glyph!=token.substr(x,1)){same=false;break;}}
        if(!same)continue;for(int x=static_cast<int>(token.size());x<f.width;++x){w.spend(1);if(f.at(x,y).glyph!=" ")same=false;}
        if(same){require(!row,"physical: ER107 ambiguous following source sentinel");row=y;}
    }
    require(row.has_value(),"physical: ER107 missing complete following source sentinel");return *row;
}
inline void check_rectangular_entity(const relation::Grid& g,const relation::Rect& b,
    const relation::Entity& e,OracleWork& w) {
    // Required incident perimeter arms are checked in addition to the old
    // oracle's selected-port and no-extra-arm predicates. No rounded substitution.
    constexpr std::array<std::pair<int,std::string_view>,11> canonical{{
        {relation::L|relation::R,"─"},{relation::U|relation::D,"│"},
        {relation::R|relation::D,"┌"},{relation::L|relation::D,"┐"},
        {relation::U|relation::R,"└"},{relation::U|relation::L,"┘"},
        {relation::U|relation::R|relation::D,"├"},{relation::U|relation::L|relation::D,"┤"},
        {relation::L|relation::R|relation::D,"┬"},{relation::L|relation::R|relation::U,"┴"},
        {relation::U|relation::R|relation::D|relation::L,"┼"}}};
    for(int y=b.t;y<=b.b;++y)for(int x=b.l;x<=b.r;++x){w.spend(4);if(!b.boundary({x,y}))continue;
        int need=0;if(y==b.t||y==b.b){if(x>b.l)need|=relation::L;if(x<b.r)need|=relation::R;}
        if(x==b.l||x==b.r){if(y>b.t)need|=relation::U;if(y<b.b)need|=relation::D;}
        const auto got=relation::glyph(g.at({x,y}));require(got.stroke==0&&(got.bits&need)==need,"physical: ER107 required entity border arms");
        const auto symbol=std::ranges::find_if(canonical,[&](const auto& v){return v.first==got.bits;});
        require(symbol!=canonical.end()&&symbol->second==g.at({x,y}),"physical: ER107 canonical normal rectangular boundary");
    }
    if(e.typed_er)return; // Complete typed interior is already checked by D0.
    require(e.headers.size()==1&&b.b-b.t>=static_cast<int>(e.headers[0].size())+1,"physical: ER107 complete plain header rectangle");
    for(int y=b.t+1;y<b.b;++y)for(int x=b.l+1;x<b.r;++x){w.spend(8);std::string_view want=" ";
        const auto row=static_cast<std::size_t>(y-b.t-1);
        if(row<e.headers[0].size()){const auto& text=e.headers[0][row];require(text.size()<=static_cast<std::size_t>(b.r-b.l-1),"physical: ER107 plain source header fit");const auto start=b.l+1+(b.r-b.l-1-static_cast<int>(text.size()))/2;if(x>=start&&x<start+static_cast<int>(text.size()))want=std::string_view(text).substr(static_cast<std::size_t>(x-start),1);}
        require(g.at({x,y})==want,"physical: ER107 exact centered plain header and blank interior");
    }
}
inline std::unique_ptr<SceneEvidence> check_complete_frame(const Frame& actual,const BoundCase& bound,
    const Surfaces& surfaces,std::string_view sentinel,OracleWork& w) {
    frame_limits(actual,w);const auto& p=bound.projection;
    const int stop=following_sentinel(actual,sentinel,w);std::size_t notice_count=0;
    for(const auto& record:p.notice_rows)notice_count+=record.size();
    require(stop>static_cast<int>(notice_count)+3&&stop<=1024,"physical: ER107 complete body/notice/separator extent");
    const int body_height=stop-static_cast<int>(notice_count)-1;
    sd::pay(w,static_cast<std::size_t>(actual.width*body_height),sizeof(std::string)+40,sizeof(SceneEvidence)+1024);
    auto evidence=std::make_unique<SceneEvidence>();auto& g=evidence->observed_body;
    g.width=actual.width;g.height=body_height;g.cells.reserve(static_cast<std::size_t>(g.width*g.height));
    for(int y=0;y<body_height;++y)for(int x=0;x<g.width;++x)g.cells.push_back(actual.at(x,y).glyph);
    // Fixed independent oracle stage, prepaid before allocations/search. A
    // resource exception remains a failed verification, never mutant evidence.
    w.spend(8'000'000);relation::Work geometry;
    const auto errors=relation::validate_impl(g,p.scene,geometry,false,nullptr,false,nullptr,relation::kRelationsFollowingToken,nullptr,&evidence->contacts);
    if(!errors.empty()){for(const auto& error:errors)if(resource(error))throw std::runtime_error("resource: ER107 whole-scene "+error);throw std::runtime_error("physical: ER107 complete source scene: "+errors.front());}
    require(evidence->contacts.valid(),"protocol: ER107 whole-scene receipt not published");
    const auto outlines=relation::outlines(g,geometry);
    for(const auto& entity:p.scene.entities){std::optional<relation::Rect> selected;
        for(const auto& box:outlines){geometry.spend(16);if(!relation::entity_content(g,box,entity,geometry))continue;
            // Complete source content, not observed color or producer metadata,
            // assigns the contour. Multiple matching frames are not guessed.
            require(!selected,"physical: ER107 ambiguous source entity/table contour");selected=box;
        }
        require(selected.has_value(),"physical: ER107 missing source entity/table contour");evidence->entities.push_back(*selected);check_rectangular_entity(g,*selected,entity,w);
    }
    for(const auto& group:p.scene.groups){std::optional<relation::Rect> selected;
        for(const auto& box:outlines){geometry.spend(16);const auto content=relation::content(g,box,geometry);if(content.text.empty()||content.text.front()!=group.label)continue;require(!selected,"physical: ER107 ambiguous source group contour");selected=box;}
        require(selected.has_value(),"physical: ER107 missing source group contour");evidence->groups.push_back(*selected);
    }
    // Unique source assignment must itself pass D0, not merely coexist with
    // some other successful assignment selected internally by the oracle.
    auto frames=evidence->entities;frames.insert(frames.end(),evidence->groups.begin(),evidence->groups.end());std::vector<int> assignment;
    for(std::size_t i=0;i<frames.size();++i)assignment.push_back(static_cast<int>(i));
    const auto assigned=relation::check_scene(g,p.scene,frames,assignment,geometry);
    if(!assigned.empty())throw std::runtime_error("physical: ER107 unique source outline assignment: "+assigned.front());
    std::vector<bool> has_label(p.scene.relations.size(),false);
    for(const auto& bridge:evidence->contacts.bridges()) {
        require(bridge.relation_index<p.scene.relations.size()&&!has_label[bridge.relation_index],"physical: ER107 duplicate/foreign source label bridge");has_label[bridge.relation_index]=true;
        const auto& box=bridge.label_box;relation::Rect content{box.l+1,box.t+1,box.r-1,box.b-1,"label"};
        evidence->labels.push_back(content);evidence->adapted_contacts.push_back(adapt_label_contacts(g,p.scene,evidence->contacts,evidence->entities,evidence->groups,{bridge.relation_index,content},w));
    }
    for(std::size_t i=0;i<has_label.size();++i)require(has_label[i]==!p.scene.relations[i].label.empty(),"physical: ER107 complete source label bridge population");
    int min_x=g.width,min_y=g.height,max_x=-1,max_y=-1;
    auto include=[&](int x,int y){min_x=std::min(min_x,x);min_y=std::min(min_y,y);max_x=std::max(max_x,x);max_y=std::max(max_y,y);};
    for(int y=0;y<g.height;++y)for(int x=0;x<g.width;++x){w.spend(8);if(g.at({x,y})!=" ")include(x,y);}
    for(const auto& box:frames){include(box.l,box.t);include(box.r,box.b);}
    for(const auto& bridge:evidence->contacts.bridges()){include(bridge.label_box.l,bridge.label_box.t);include(bridge.label_box.r,bridge.label_box.b);}
    require(min_x==1&&min_y==1&&max_x>=min_x&&max_y>=min_y,"physical: ER107 exact one-cell top/left body apron");
    require(max_y+2==g.height&&max_x+2<=actual.width,"physical: ER107 exact cropped right/bottom body apron");
    evidence->body_width=max_x+2;evidence->body_height=g.height;evidence->total_rows=stop;evidence->intrinsic_width=evidence->body_width;evidence->source_relationships=p.scene.relations.size();
    const auto roles=primitive_roles(g,evidence->entities,evidence->groups,p.roles,evidence->labels,w,evidence->adapted_contacts);
    // Error-only bounded context; preserve the exact predicate and resource
    // classification. Color strings are hex-escaped so diagnostics carry no
    // terminal control sequences. No pixels become source expectations.
    auto check_at=[&](int x,int y,std::string_view glyph,CellRole role,std::string_view region) {
        try{check_cell(actual.at(x,y),glyph,role,surfaces,w);}
        catch(const std::exception& error){
            if(resource(error.what()))throw;
            auto hex=[](std::string_view value){constexpr std::string_view digits="0123456789abcdef";std::string out;const auto n=std::min(value.size(),std::size_t{48});out.reserve(n*2+3);for(std::size_t i=0;i<n;++i){const auto c=static_cast<unsigned char>(value[i]);out+=digits[c>>4];out+=digits[c&15];}if(n<value.size())out+="...";return out;};
            const auto& got=actual.at(x,y);const auto& expected_bg=role.inherited_background?surfaces.inherited_background:surfaces.background[static_cast<std::size_t>(role.background)];
            std::string detail="physical: ER107 cell at("+std::to_string(x)+","+std::to_string(y)+") region="+std::string(region)+" body="+std::to_string(evidence->body_width)+"x"+std::to_string(evidence->body_height);
            detail+=" expected{glyph_hex="+hex(glyph)+",fg_role="+std::to_string(role.foreground)+",bg_role="+std::to_string(role.background)+",inherited_bg="+std::to_string(role.inherited_background)+",fg_hex="+hex(surfaces.foreground[static_cast<std::size_t>(role.foreground)])+",bg_hex="+hex(expected_bg)+",bold=0,dim="+std::to_string(surfaces.inherited_dim)+",other=0,link=0}";
            detail+=" actual{glyph_hex="+hex(got.glyph)+",fg_hex="+hex(got.foreground)+",bg_hex="+hex(got.background)+",bold="+std::to_string(got.bold)+",dim="+std::to_string(got.dim)+",other="+std::to_string(got.other_attributes)+",link="+std::to_string(got.hyperlink)+"}";
            for(const auto& bridge:evidence->contacts.bridges())if(x>=bridge.label_box.l&&x<=bridge.label_box.r&&y>=bridge.label_box.t&&y<=bridge.label_box.b){detail+=" label_gap_relation="+std::to_string(bridge.relation_index);break;}
            detail+=" cause="+std::string(std::string_view(error.what()).substr(0,192));if(detail.size()>1536)detail.resize(1536);
            throw std::runtime_error(detail);
        }
    };
    for(int y=0;y<g.height;++y)for(int x=0;x<evidence->body_width;++x){const auto i=static_cast<std::size_t>(g.index({x,y}));check_at(x,y,g.at({x,y}),roles[i],"body");}
    int notice_y=g.height+1;
    for(const auto& record:p.notice_rows)for(const auto& row:record){evidence->intrinsic_width=std::max(evidence->intrinsic_width,static_cast<int>(row.text.size()));require(row.text.size()<=static_cast<std::size_t>(actual.width),"physical: ER107 complete notice clipped");for(std::size_t x=0;x<row.text.size();++x)check_at(static_cast<int>(x),notice_y,std::string_view(row.text).substr(x,1),{1,1,true},"notice");++notice_y;}
    // Source-block exterior tails and separator preserve the explicit inherited
    // dim wrapper. Footer/below-footer rows are outside that wrapper and undimmed.
    // All regions have exact baseline glyph/FG/BG/bold/other attributes.
    std::size_t flat_notice=0;std::vector<std::size_t> notice_widths;for(const auto& record:p.notice_rows)for(const auto& row:record)notice_widths.push_back(row.text.size());
    for(int y=0;y<actual.height;++y)for(int x=0;x<actual.width;++x){w.spend(4);
        if(y<g.height&&x<evidence->body_width)continue;
        if(y>g.height&&y<stop){flat_notice=static_cast<std::size_t>(y-g.height-1);if(static_cast<std::size_t>(x)<notice_widths.at(flat_notice))continue;}
        const auto want=y==stop&&static_cast<std::size_t>(x)<sentinel.size()?sentinel.substr(static_cast<std::size_t>(x),1):std::string_view(" ");
        const auto& cell=actual.at(x,y);sd::pay(w,cell.glyph.size()+cell.foreground.size()+cell.background.size(),2,16);
        require(cell.glyph==want&&cell.foreground==surfaces.foreground[1]&&cell.background==surfaces.inherited_background&&!cell.bold&&cell.dim==(y<stop?surfaces.block_exterior_dim:0)&&cell.other_attributes==0&&cell.hyperlink==0,"physical: ER107 exact separator/footer/exterior attributes");
    }
    return evidence;
}
struct Mutations {std::size_t semantic=0,physical=0;std::vector<std::string> executed,skipped;};
inline Mutations exercise(const Frame& actual,const BoundCase& bound,const Surfaces& surfaces,
    std::string_view sentinel,OracleWork& w) {
    auto positive=check_complete_frame(actual,bound,surfaces,sentinel,w);Mutations result;
    auto rejects=[&](auto&& run,std::string name,bool semantic){bool rejected=false;try{run();}catch(const std::exception& error){if(resource(error.what()))throw;rejected=true;}require(rejected,"protocol: ER107 source/physical mutant was accepted");result.executed.push_back(std::move(name));if(semantic)++result.semantic;else ++result.physical;};
    auto semantic=[&](auto&& alter,std::string name){sd::pay(w,model_binding(bound.projection.scene,w).size(),8,sizeof(BoundCase)+4096);auto changed=bound;alter(changed.projection);rejects([&]{static_cast<void>(check_complete_frame(actual,changed,surfaces,sentinel,w));},std::move(name),true);};
    semantic([](Projection& p){auto& e=p.scene.entities.front();e.label+="!";for(auto& h:e.headers)h.front()+="!";if(e.typed_er)e.typed_er->header.front()+="!";},"entity_complete_header");
    bool table=false;for(std::size_t i=0;i<bound.projection.scene.entities.size()&&!table;++i)if(bound.projection.scene.entities[i].typed_er){table=true;semantic([&](Projection& p){auto& f=p.scene.entities[i].typed_er->attributes.front();std::swap(f.type,f.name);},"typed_type_name_column_identity");}
    if(!table)result.skipped.push_back("typed_type_name_column_identity:no_source_table");
    if(!bound.projection.scene.relations.empty()){
        semantic([](Projection& p){auto& v=p.scene.relations.front().start;v=v=="card:one"?"card:zero-one":"card:one";},"source_cardinality");
        semantic([](Projection& p){p.scene.relations.front().label+="!";},"complete_source_role_label");
    }else result.skipped.push_back("cardinality/label:no_source_relationship");
    auto physical=[&](int x,int y,auto&& alter,std::string name){sd::pay(w,actual.cells.size(),sizeof(ObservedCell)+96,1024);auto changed=actual;alter(changed.at(x,y));rejects([&]{static_cast<void>(check_complete_frame(changed,bound,surfaces,sentinel,w));},std::move(name),false);};
    const auto b=positive->entities.front();
    physical(b.l,b.t,[](ObservedCell& c){c.bold=true;},"border_bold");
    physical(b.l,b.t,[](ObservedCell& c){c.dim=c.dim?0:2;},"inherited_dim");
    physical(b.l,b.t,[&](ObservedCell& c){c.foreground=c.foreground==surfaces.foreground[2]?surfaces.foreground[4]:surfaces.foreground[2];},"complete_border_foreground");
    physical(b.l+1,b.t+1,[&](ObservedCell& c){c.background=c.background==surfaces.background[2]?surfaces.background[4]:surfaces.background[2];},"owned_interior_background");
    physical(0,0,[](ObservedCell& c){c.glyph="X";},"unclaimed_apron_ink");
    physical(0,positive->body_height+1,[](ObservedCell& c){c.glyph="X";},"required_projection_notice");
    if(!positive->contacts.bridges().empty()){
        const auto contact=positive->contacts.bridges().front().left;
        physical(contact.x,contact.y,[](ObservedCell& c){c.bold=true;},"same_relationship_contact_full_style");
    }else result.skipped.push_back("contact_style:no_source_label_contact");
    return result;
}

} // namespace loom::test::mermaid_er107::authority
