// Test-only independent Block58 source authority. No renderer or filesystem access.
// JsonVal views borrow immutable caller-owned JsonDocs; callers own parsing, file hashes,
// capture ordering and reports. Callers check original manifest bytes against
// kManifestSha; bind independently pins the selected complete manifest/Git record.
// This binder grants no drawing/runtime admission.
#pragma once
import std;
import loom.serdes.json;
import loom.crypto.crypto;

namespace loom::test::mermaid_block58 {
using JsonVal = loom::utils::json::JsonVal;
inline constexpr std::string_view kSourceFactsSha =
    "1d0006ab73cf61951a15599606cde2e46a59865416ac9f43ad244d45b0e4c27f";
inline constexpr std::string_view kTerminalFactsSha =
    "b7d0bf628af004718df6ab3550dd09bb0a71481bc2f70b0cf3e47b2584870caf";
inline constexpr std::string_view kManifestSha =
    "c966a7258f1007eda6b992be7a79c2d5f746169d21591dbdd0b7091c6371569b";
inline constexpr std::string_view kSourceLock =
    "eb887a461f7194023f7622fb823af3d02d5cb17bb2decf19c4b2b22bf204a877";
inline constexpr std::size_t kMaxFactBytes = 4u * 1024u * 1024u;
inline constexpr std::size_t kMaxRawBytes = 64u * 1024u;

struct OracleWork {
    std::size_t left = 128'000'000;

    void spend(std::size_t n) {
        if (n > left) throw std::runtime_error("resource: Block58 oracle Work exhausted");
        left -= n;
    }
};

struct BoundFacts {
    JsonVal source_case, terminal_case, source_model, terminal_model;
    std::string path;
    int global;
};

struct Pin {
    std::string_view path;
    int global;
    std::string_view raw_sha, source_model_sha, terminal_model_sha, witness_sha;
    std::string_view manifest_record_sha, terminal_source_binding_sha;
};

// Generated only from the two reviewed immutable assets named above. These are
// independent literal pins, never accepted from a caller's recomputed digests.
inline constexpr std::array<Pin, 58> kPins{{
    {"block/nightly-check/block-arrow-should-span-multiple-columns-when-widthincolumns-is-set.mmd",
     58, "904cf163f79c7d29232b220794a93232a774f3883022858e9744a9522ecd557f",
     "cb1383dfe0d206aeab90a93f0f7c8da38208db205b2a393712e5c950e8ca2d70",
     "13f898e7c698e4daa81e39f612edc87712dd0d9c9c18950772f35c00495f960e",
     "d23cb823b5d6d57161b367a53aa94607e5b093a5e3bd8204b23e27eedd0e00a3",
     "ef10559a909ab8fc98f8bf149752048cd0c9fd29edd3540e467186912a10f80b",
     "a280925b165e0969ad6e1b6709186b1aad36adbcda4efa99ebe8a7baf6e58657"},
    {"block/nightly-check/block-in-the-middle-with-space-on-each-side.mmd", 59,
     "b238acd094cee7f0fa9ee4077a71f98fca7ed2e30c9a2dd0d26a073d79c79495",
     "89350b78c86439e346a2de5bc0d82da70baa17614a01783ee7cd2d7f4f148e6a",
     "71d7764ba039d5a3a09f07073a822f78997030213e1803fe448419269a8ee4c4",
     "d92befa6d41f13b5f426f13d1612b2a1a3a5107fca9e766977e92279d1218c4e",
     "eb008725bef90190622b551aaf710ad74ac8e3cbc5b29e2b62491fc2ab007e56",
     "76dc27ca022cc854f677ec1d56c7bb3d639507641f03ec705c9aca26da6b5e74"},
    {"block/nightly-check/block-should-overflow-if-too-wide-for-columns.mmd", 60,
     "c0d84d372b6f1ef8a6515e4404284485d2372505a0036d9020619851753f9f72",
     "3bdd6e96aab1c8bb97b15f1b433dfc038560fd3858b29bf6b777f3422a91e4ab",
     "62bacd96bff03abb6e0d814602c6153d9c91e211bb3476852f8dc309284dfe15",
     "25f0ab24e39e8e2386aa13a7a88f012e2e7ee41e164a9c1f97944ed33d549d69",
     "c50250b5d98250a18362db9ec118712d91f4b01217deba74e3b4758dc8342792",
     "be760ec74e45c23f09aa23e392fb9d14f1add7934fb5e2f75d644b939f1324c1"},
    {"block/nightly-check/block-sizes-for-regular-blocks.mmd", 61,
     "1183af55fa2ed866308b286bdd802a2cf2147679d6e773c4362d356b292eeed0",
     "f6d3d74b080bb310ee65f06704db7db0b3c64c1b789bf35cc25cb4ccd2073375",
     "b9a38f4ecc48544b6a066499541290bb461edac14d98fb9c9be5c7e2231877fc",
     "2524c39c009415c6d2a782e6e2b65cab475130f935611c95b9ba49a33c19b3f7",
     "000e7491652d4caec56030ab79780f5d375a069e2cd001ee5ae1117825179e84",
     "da635eebf6aed8c892c394a7035dc059f6a4a1bc99bfd47e6bc7cdc077d4994d"},
    {"block/nightly-check/block-types-1-square-rounded-and-circle.mmd", 62,
     "798801fa4313725d6d3c87e0e2d9f2bb2ea59ff927f0234f2531530e70d3a02d",
     "25bb72b21dc82805e324f93e9b4bdb5a740c37565e55a0386da0c8f1da5ee23e",
     "e4b58964dec5742b390c2b8986e24b1ef888353e40ab2201802189d90e4371ca",
     "1463295ec3880e9f1bc8f67dbc20dfe0e186159cf4d58275eb01aa96e0e9f80a",
     "e4979d656f38678e5c16026f751763491e89d6ce9c0f06b4a8d73d94fd0741a9",
     "697c295f501fdb43234621ee57361f5e6530e37754568a9f79c8f74aa96dd00e"},
    {"block/nightly-check/block-types-2-odd-diamond-and-hexagon.mmd", 63,
     "888b02c7045456cc32494fe4743edb3708db0b5926dbfc13388e28a2e369f6b2",
     "648481fd0ce1194543eeab0b06b7d65fe4c2611a6a5aea278820c5f0d100cfc2",
     "c3dd11e10bb2e1ddba96bfcd8180d0656307479a810ad7c4fa095cf417fad674",
     "84907f71a4ae5da7c4c31d1f95f1ede6c0c062da903e77003fb0ee1b3c0096cc",
     "8e6e354c4ad5ad63e192ed4fea5b22925e694e036ae9f3b1f26c68aaf1ac5443",
     "cdda567ce33563835e385eb851a0b4bd12404d6ecea305b08b8bf03b84a0024d"},
    {"block/nightly-check/block-types-3-stadium.mmd", 64,
     "b15533ae5dbaf01bb402e539d2b29a608c7d3fe09af9f006751031268959521e",
     "8d14b9a370c3d4beae3b4f1df929b1564567bbe515a6a6e65aa32acb11a840db",
     "0f55ca748422d0388d88cd6d9fc91cab507e35c09619136d59f4bf678ce48896",
     "e0c0dd730a38ebfd51f63ced90278b2f730b934b7c9dff18891d387d139703d2",
     "62742d363fa8ee376810705993238aa387fc58b67386e7cf0b76de5c9b9c6541",
     "a27895e7fdf84225a84789223a3c8feca9af9f7e7746f5e8f5c798b1154341b6"},
    {"block/nightly-check/block-types-4-lean-right-lean-left-trapezoid-and-inv-trapezoid.mmd", 65,
     "d14041d7e755861b9c2d8cfb63557be05e46749b3515f1a6f8f285c9051258b9",
     "3b9ca68881129170137bc2993ecf42735a941b972e5e405f96d928b78b34db2e",
     "b837d73c218c1798406f1e68b4416bd2dc8cbe5a390627b3c79b75fbc4b15fa3",
     "4a18abc17f6547507516d459c46095b08ee2a27f61a0f198bad27ed2fb75caf4",
     "a7f6d6914426a04e424c86018f495c07d115f374ccdf05881b760e0c853f0fd3",
     "d91313ac85fb0052ffcfcf352f37b3f9661b3e4490ff2ca1b1506dc53b3fad19"},
    {"block/nightly-check/composite-block-with-a-set-width-f-should-use-the-available-space.mmd",
     66, "c718405ab333103bf5f821bc10df8a69ba7aee0681762481fc093d051c4ff6a5",
     "a689cf5f120970d2af562878c59439e8f79d878a82c40a1bb5fd6f42f6ac4c06",
     "90a2f6b0e8905d1edc29c948c4f144fb2f7365b9960c79daad18559406d8f555",
     "f3b3749aa5fb862c3ac8a9084e851ad3118a294fa6dffda51fee8c50452c8e12",
     "3738e6a258de4e14e3c269ed3356f17c3c100d92884fd64fd05447ff8a84789f",
     "2453b9ff14dc0354e87e9856148fc9747fc3b234f1e049eccefc883efc51095e"},
    {"block/nightly-check/edge-without-arrow-syntax-should-render-with-no-arrowheads.mmd", 67,
     "068d0942d70fc43e852239db6fc7b5befc134e0f69b3fa112d1342262928f9a4",
     "51932451663f3173dbfed03c8b9bb8dfccab421af28532bb084323aabb1990e9",
     "8111600bb939f0b62c3de4d49fde25d5b7a3f92fb34de97ff7faa2500e73fdd0",
     "5e738aefe57b7677c7a486be9343cf8fc0d9e7878decaa2f3e1f836080c0281b",
     "8633e171c1473261333e0b4652355d240c761a47bdc5a87782cb5c6d280adcdc",
     "6d09544b32bac639abdd9deb58e733a1094e9a8b54051a143b904a923b69ff33"},
    {"block/nightly-check/edges-should-handle-labels.mmd", 68,
     "cf7115b1f6f9c4bd4d1935dc792077649e6125b9d726f4d9c1531a1e1f25fd8b",
     "6e1e28dc5657dbf3009a1bd9bb6bff8e8a5eb353f0bb7a3446179b9c73ec9a85",
     "799550e63c21aa537e0e192989623b4be0d1c40ca7c5e74bdcc63bc1fc3cb17e",
     "3295f756dfd7181d92a4e7ec041f1125d5bb4eac15f3e531ff859a5d1e10251f",
     "1886bfc62d4a247bc5074aee1f51d8055719b471961ffd8eb2499c002b96387b",
     "c2ce1d01ebc0c0df08d62e7d06b59204a36f9bbae50941c51ff48d129093df7e"},
    {"block/nightly-check/should-align-block-widths-and-handle-columns-statement-in-sub-blocks.mmd",
     69, "c1959107259a00a994ac9587d8df81d64c2809de59aad0af27db280f7a3ebca4",
     "c38245c1f10e6c4fb9d8c8b648db5514361b3091df97a17d709ac3fc70e5a3f2",
     "ce331ee289c2f801bb2898c823f1d7bce90bb1f6c33c7f7093306a21066b6efa",
     "1e09bf17b4757b4dfbfb52a9a6ecbfd663473435ee3c82936e63c44bda4c6d21",
     "dfa3330ec551a8a0d4e331f54df2af988a740aa4cbd4f740349b525ec194d71b",
     "dc732fe714a09d30a1d75301d770960852e3117a82d416e70f96a1dee2ae8480"},
    {"block/nightly-check/"
     "should-align-block-widths-and-handle-columns-statements-in-deeper-sub-blocks-then-1-level."
     "mmd",
     70, "4e48c32984f7c3851b61cc430c37905c7a0d2f65ffa78ed1a689d8d371d314c6",
     "b7b716a9b2890f5d3f1540da419a7e9b4f05fc253cc1d33c5fb4cf366b9f5b53",
     "de8f635bf1d681de5df07b3eea9737a0becd02289a9ecca8193983c073cbceaa",
     "be33aab6654af3c3e95245f612f7b957e96183a07134f2f30958c2b09fc8f2d7",
     "cea8d071a412974c431a26164b7a176c65b6a059b9a1edf316d1ae2f701d97a5",
     "bcd11c938f268c5fdaad4bd1404801ac34b8939fd4d49ef221298258290f18a5"},
    {"block/nightly-check/should-calculate-the-block-widths.mmd", 71,
     "bea42317d1d144a3d8cc186c5d550e00b9f8fe3c0cad02f679e6f8f21b620b12",
     "fb063b96513062228a457ae3a18b559c97a7ce7d5d145ed5977c79898dfa7721",
     "2c708b27b88a323703f7afe349ea35f039f3ca3f362059d9b00068f7fc52e698",
     "3c9af766bbb37472e52c881ac1e5ad5bcaef3a682bbcad1cf9965feee8eae5aa",
     "98710c187b0cf17b49a58849e2ba9b49e438a0045dd8fdc95f43d1b7c37af105",
     "c896ffe0995f186ddaef93443fd754b4f1c6410f758bdba0b9aecfa630c5c0ad"},
    {"block/nightly-check/should-handle-block-arrows-and-spece-statements.mmd", 72,
     "1e7df7bb0f2375c21a1e15ae9313e8ed625be3d15d7dc3f0765ffb7fd12a2cfe",
     "382c62ae62f0d514d66b4f61bdb97e8f604842c0709861418a22c416fdfddfa2",
     "e8a63af2dede8b2458e4faff0bec84a9bb5231a3409a31d3c9f1f78878bb9378",
     "a07956274ad8728a2893217c1fe153176eb27301f464b0274c6cf4e8e61609b4",
     "554c3d624ed79b8bd7da2ebf5686f6ad5662baedeab79ad87e70be212d705090",
     "48037285a26ccff2bd321d16183b0d286f4dd49bdb6c11af61e2a33a50257fc5"},
    {"block/nightly-check/should-handle-block-arrows-in-different-directions.mmd", 73,
     "1f412a4f9962424b49a24466f62046d76662209221cceef0898f422e22e5976f",
     "7776080316ebd0d80c208d2e3ce3b0d5c0d90ac1f5a6d93bd41dd6ecae79c2db",
     "d3ae65337c691402051517966721ffdd564b2143cb2c1a5db477372942f6eaf3",
     "2c82bd7f0780eafb37eb26c789d594b5d0f282c584d384ed4141f6abf38ac8a8",
     "e543a0312ff9c6548ec444b8a7b5ad9f25a2257dc6c92340866db4fdca499ccc",
     "577720ee0d576da6736613a71d7c0617168fba96e0c899b0f033a60bfb608b53"},
    {"block/nightly-check/should-handle-columns-statement-in-sub-blocks.mmd", 74,
     "a6a704b2f62c1bc61a886a3a192ffea38be8a3012ad99274ea21178c5afdb9e4",
     "a1c3e6bc009a6a483a17accab36a1576d88145168ed374e6ef96dead4cd22b6d",
     "83f9462b81720577b60c61da3b14fcb3f4ab6194a9a3501c2f53b89641543473",
     "1e370a33dcec332b1207b34f1f5c77e24d05a619eb628d0f484ae7b8838f3fe0",
     "34de2f7801b093e4af20d70ed94969f12a7bde4081232e0b752904b2268a5870",
     "bb8682a37ff81f2bb71796b4bc09080cc39bb325622ffeb599d60aa3f824d198"},
    {"block/nightly-check/should-handle-different-types-of-edges.mmd", 75,
     "0ba840aeb452c5f40b323ba4d1c0146ce4829c405b72dbe0b4a338d5f7086c18",
     "041c4dcb2222b7014f5d5d88dde646be67cf6a6b35f535bda4f542ff649b2309",
     "14f80ebce77fe78d67697b3ebf71466bb397831707c81a2bf3a22595cff8ebcf",
     "3ef31b418110bc9f9bd57a584ff0423bcf7fb0b5e58e300066aa827b2e092793",
     "1c2a2fbadb0c4a2cf6370ead5fc77e7407fd740d389a0466f7034b18103dfd22",
     "27a671dd88366c47d2bf3edbd83905a607eb66710d43a3dbf74c1c68ec935c77"},
    {"block/nightly-check/should-handle-edges-from-composite-blocks.mmd", 76,
     "f2ca21833eae002c391da76c3d695a96fce99676e4175d51604d7fff6f0712e4",
     "ed6dbfb8ce5024f018cf37e2c6f2ed6d2976b9eeab5768baeb0401407477084f",
     "a1d78838adb91e4ed1d2082f2b92844d1d1fc63ed6698eb381ee177db112a2ce",
     "405a9fb4bb63aa728fe20ff4b068309eeca9bb33b5f04ec17c00fcd66da1972e",
     "50f81782e3570f74e20b104445289408aaac5f014f6544778b0fe402089637d4",
     "1cccb367379c56e0eb2d412c33630b363abc2c2e10eb11a9c44a7c73436d9106"},
    {"block/nightly-check/should-handle-edges-to-composite-blocks.mmd", 77,
     "d3fd4903b71bfed053a2733ce546fa68d217497f21e585209a585015afc84064",
     "1715c25133bd74e5b1a407d60f5c80061454f1999dad1c3b436285e2c8716e82",
     "36f6bc2b5eb504f054a05fdfe95e690c0b27ed15a0bad1eadb59f29140209348",
     "cb12eda0748dd70d18b1cb33053a54a7b5d291470681df89167ceadbafea00fb",
     "00eb0084abf0fafef81e693403690d1bdc9edaad1d4e6aabb925742908dda2ee",
     "0ff93b90f45cf9d3f7ca83f7422dd0d4caacff7d123231c008f14c51d7c42500"},
    {"block/nightly-check/should-handle-sub-blocks-without-columns-statements.mmd", 78,
     "69010cd44a24868b53c2ccea5ba3e05ea2e0aebd3e2c7e38f3fcf75db4f3a838",
     "ea49a58590e51a34948cc8f1bdb1eb4b3e79449500db3ed96f84554f98ceed29",
     "a33eca7fa1bbff4cdddde8f707f819ce691d1b5c0a2aec5dfb4362d900993814",
     "c254680a0ac47bc7e56996e41236ab98bcaeaa02e08a67ef7b1e978d979f6715",
     "819a0b9ce3b181087d56e8ab536becc5fec530703731fa84975be9405d2630dc",
     "a3cc12ca260dec97a9f7047d73886dc8cff4b23b9f541c9787080ef958c2f6b5"},
    {"block/nightly-check/should-render-all-arrow-types-in-9-columns-grid.mmd", 79,
     "79ac3fdccd89a017e18faba9c5a788eab9d1b1a04a38d55b946fc44e57250e19",
     "c119f3cfa156fc4cd92affd56fe2eff28974a5d376e5367cd538258b9dd2f842",
     "e3f0b9c5c76642b716b5f693ade81b3ecf9de964ff7f2484740bb3373b62efd2",
     "172e865a958f57681b491f7690d4a4be7ecc03e0fa9ce50db75b13a24c905a4e",
     "b770ef10e37698e30f6fb0a56aa6f488f0d1945c5e27d3853606d56fbb8aab94",
     "0e0e1f8edbf35f5dd82994acaca2c974412b140da5b08ca09dc8461d28c78da9"},
    {"block/nightly-check/should-style-statements-and-class-statements.mmd", 80,
     "82eb3f54b670df9c55b518feafaf14f918288e8842b37851945b2835280cdbf5",
     "92036b6c0c4d03f8f702a37aeef135bee327e3a8ccd6c1d0a14d113bc3a30e78",
     "7d59857f0d88430b156341180e41b5df2a929edd58fd5f2f3244eeb9c98931c0",
     "f6e2904e22db1cc7f43a82596544691d644388a0c626fe64552bee967fb38115",
     "18c5ca4325618dda223af5862d13063cb9a0f5c3009b72f39d9aa996c4ae25db",
     "92b91933ecb8bf2ee3eeda90329dee15e7439e10420ddd4decba236604ec0396"},
    {"block/nightly-check/sizing-it-should-be-possible-to-make-a-block-wider.mmd", 81,
     "2ba6fdcdd3df7eea395e8518e06971b2a52a5057a3f049c666dee8769b3b80a4",
     "748fc970660806af47f4681a92a9f6ce5864c2ca2638f0dc4d6fcbf0a07533e9",
     "86a700f38029c85a1b4b8aa4cabcdf3cfe089488d0f2a1a5c0d8c1be7a4666f6",
     "48901321e660ed44b9d1ce4f1d51b241367756bce46cff04d31e4bb8dc5aad6e",
     "173bfac34d57c4bb6e836b70318f70df4b4aaf5cc28f22ac5a7775d3c8012313",
     "30575178bb48eaf54d8368238b9699b7ddacd22f33fd520d13a175b3b6e66171"},
    {"block/nightly-check/sizing-it-should-be-possible-to-make-a-composite-block-wider.mmd", 82,
     "bfc6395b8b56c6585016f6ab2880a1b5ee02f6e4f56c56fa328d2ee46ab28448",
     "e138dae176822015117320a22da42869ecc7ec089292b81c7a57be1cc587fb7c",
     "085ba244e61bffcac1ab9fd83606b930e5a6a03f225650b5e56cff198414f156",
     "6f28e158703634a9a729d7772cc7f9f11aea880c84707f9837d77be8cbd3355c",
     "eeeff8d8a7ab2ae60a51f8a6cacd0c0919b961debecb3556e5b01c638cd5f3ad",
     "1b1e58172af56cff54f5a8bfe45e3c65e04cbdf601e6c78a998066d7d890a85e"},
    {"block/nightly-check/width-alignment-blocks-should-be-equal-in-width.mmd", 83,
     "837ed36723b30a8ffec5336fd73fba2722cc5d840f28837ebd06c693512ae7c7",
     "3cf90298dd405c3c158669eddfe2a604cc4970ca7ad6dd2cedb718ad5c09534f",
     "bf009557202e8383f4a986025abb62794975f1afc27208db273d031de0b1f306",
     "96a581bfb8234882e2e17ca818606bb989d2faeba5cedf0eaaad1166a7d23fcd",
     "c03c863b41d8778fc6ba3e927ef2579fdeaaac04bc9578573ee0de3619ca9c35",
     "b637d07cc685440ea86558b42e33378f8a91b76a223194532674fbac28512b23"},
    {"block/nightly-check/width-alignment-c-should-be-as-wide-as-the-composite-block.mmd", 84,
     "40165a58e937936e20b97d637f7d04dbf66f8414e09659461526d80cf7d3ccb9",
     "03bda265baf60d18f5e776ac7fa08a838e9c2e1a0f86fd2fcb89d21d2bee35b3",
     "c6fdb7a02a3277de502182679a4449c294bb34b5eda742c935bb23fe7e0e35a2",
     "2b1fa8f3b06a3572f9be4b40e50db89b63214922719a420d65fd42385e2fc3f0",
     "7ef2b68a26fc3c2912621b04c5c20a00a0256def2b4bc18a03979d223dd05673",
     "b74d1dc643d939ee7cd9b794ce2c6f388e6f03a4aa8c4862789903847de5265f"},
    {"block/nightly-check/width-alignment-d-and-e-should-share-available-space.mmd", 85,
     "fefada2c9812c3f8c68679bae3b953cc1cb20d8e3fbf0a3648a9d09382513bb2",
     "980690be0e86cbfe3dc9ba5b8b35d8cb9836394b9658fd1b5b20d1c879a44b0a",
     "c043f95c2b166c2b561e53b5801ee3ff6e34b14c3d9b19bba2af3dcd47631aa7",
     "f21125e13ab22106e2936b9134796a500412fe92d9cb86fcea822bfabf8d7f6d",
     "e6a92645dc62545ab7bdfef3678f2cacd45da70b719e97fdc36722d43d93d6d8",
     "acf71e8897faa997dcec41c2d1380c94ba9f2d103a5fe6ae5b538829fa56a322"},
    {"block/pr-check/composite-block-with-a-set-width-f-and-g-should-split-the-available-space.mmd",
     86, "e6bf651d32dc39f2f192f88f6a0c93662e27ade5697c396970ea9e77b3bc7932",
     "fd9caac4f6ba3efdb7017793673a486128b6c0faff153f495bc422813c8d6692",
     "a32e87b388bdcba7025277af3c83b6852df8d4a328917643604ac6b3507055d3",
     "9ee686e859935e7b906aeba909f8ebe4bb5cade424dfd98458cb8f7433ae16e9",
     "d6dbfe4233f610443fa70a03696df8b1ab583180989b5e63c16edaf644a76db1",
     "767a10da4e2507df8553ebc3d564695ff7c57d0be2a839fc92d5a3f436ab7205"},
    {"block/pr-check/hexagon-shape-block-should-span-correctly.mmd", 87,
     "a6c497ec1354e5dddffe785c496e1f7c412aa9d7ae31fe91a5e24f7f5e4a9ad1",
     "8ce2c942006329362f2a317d9c302f52184ff8e52b6dd1bcdf32fd38b16355ca",
     "ac2f1f9eb9e4c983f8a31e3d5b20f937edf4a7672d4ad96e563ae6c99e14df14",
     "b05ce6fd183b2e8c970b670025a3a3b98e3220885ff42c571c87031850074146",
     "e313564b85fb04ba54ae23ada22cd6cd96a8270bdf189363a6d47665fa3f36dd",
     "28d3213281f3b66b68472f5834e764aad909829273dd4f2572aa8301029a9879"},
    {"block/pr-check/look-neo-with-the-neo-dark-theme.mmd", 88,
     "abb35a2190b635519c0895464f0546a52156cfa7e39eb89a26b18ac521a47858",
     "83996e25c4a024109329abb2d0553e568bfd2d78c8eff0ffeef95f06ee46bf39",
     "21ae7deef01c8bcf369b61d646ce0a66517ec4ede1186976ef2747dc4381e1e0",
     "0661765fa0fc8f7e2eada4bd4c7e55dbbb67814f470b347b8b0753b7aca177e3",
     "360ecb6a154dbe42338811db67b9cf23dace50fdd758768add412f65d604808e",
     "51794874aa4e01c1b98176471e5787fde1416e829f9dfcda83a9468624fb1f09"},
    {"block/pr-check/look-neo-with-the-neo-theme.mmd", 89,
     "569dc2014eee1d78ca372b7d8b9792cb41b917f5e7003afde77a60abcbc18c5c",
     "85de461e5ae457c59acd86876266ba3d22ff8058bac9a6e0c3c73a846b70bf11",
     "29e7f385f444d9607d09b84774b613afd05600bdc31ed35c6976171777a32e6d",
     "a222fc5a6ab1edfec5f4d4dba9754a7eb93fca09189aed2252da0ab163eaa40b",
     "eabf942e3a67194293ba7c0d9338c95c5cf5e6a69c3a710299a4a240adcf8bbd",
     "932fe37ac95f4fe5fce43c961d00563399a0d710d6d4bee4d4f595dc4c82203f"},
    {"block/pr-check/mixed-column-spans-should-not-shrink-column-widths-issue-7503.mmd", 90,
     "6c70c1c15351beae36baf0ccea69756dc1f36a69bf82bc4e69636e3ff4399eb5",
     "2d454ef7a0b6d9d814771bea2a3bfb9529d2263aa0f562ba0df2d82d7d7135d9",
     "d86001d0e7288ce45ef08e516f34c3720d8728be77373578bf3c8ec2fe60c0b2",
     "16c8f7d135caee10342f49f4abaefa68f180ff8cbb35dcaeee4dd6bee67fce90",
     "e13994e5386bc88b3357b81edd6081eeca5eec7e548ed0a9fc0c9f6df918eb04",
     "acf29922338c1104ceaadd7ff38eeed658ed11e342cea0de853950b2988d8cc5"},
    {"block/pr-check/nested-blocks-spanning-columns-should-not-overlap-issue-5706.mmd", 91,
     "fdf3db827339351c82d69f98667e5b94a832dda642a705b7543b214c00718508",
     "7ee81826e0a8a037ef1c16be8202defa7894f3a9472ff269887e1ae240a66e3e",
     "351a94daa9b9532623f4bbaa55331035edab0758978832bd524f0499c185248f",
     "20e979ced2023c02411d8df8f46dcf61219149591d4ffb18b5e3b0c68e7f21fe",
     "4bf70298b4206ce738cb1f134f457bf7aa7e1a87ad4750e6611a11ce173f31c8",
     "3fe4bed4f7897fc77de5ebb5203c9f233ca4f3206331942d1980f82f6b98a552"},
    {"block/pr-check/rows-with-different-heights-should-not-overlap.mmd", 92,
     "d161b54474604625b2cc46bcd01bec6edc1708b7d9884dfe341b1439fbf7df0a",
     "0262ac95a72b1a898e735ec7469b9c34274a0dc65dbf3550e3e14d1394aeb4b9",
     "b4b4be95e74f342b5a57b9d135c184fd194659d4d3c338b93f97cb9ba14862ff",
     "f582fdf2ac2da0b6bd79f93fe435371f974f45d2695c8aa267e605f2a0749a3f",
     "672d915656e6c4823070fb6e3ff40062f0741d36a3c639a726db5cd27e5b2ac9",
     "2a31ac13f796270dca68323400af4695b98f8caa69857a2aa3036c46253ddae3"},
    {"block/pr-check/"
     "should-align-block-widths-and-handle-columns-statements-in-deeper-sub-blocks-then-1-level-"
     "alt.mmd",
     93, "9a7a82dbd0bf13f90c02f5227c92406ceb9f31888f9ed49035005774175b05c6",
     "c19aae0ec5db4c270dbd7bef363f4eafe2cedebab0bae85ce0f79920662289bd",
     "1a08d41edf177b0c661266f44a7d9285ba09d4cd026dbf8403e08f4c3c371bb5",
     "d90c0a5c98d67a8336482d203185883502a9e0d0659cc821ef62032bff1fd601",
     "603165fae097ee1070e32d9cfd2c5d4499efef0e85cc92e2c3a79f9db6a503b3",
     "3ef0960b77609aceaf5d2ee53c07813240039579896c752ef10bdb54dc646f17"},
    {"block/pr-check/should-handle-edges-from-blocks-in-sub-blocks-to-other-blocks.mmd", 94,
     "f8ca9fde1149640adfb3038b40ab09c2bb5459f3e94c70ac655baeecb4aa5dce",
     "45cbc28e72df71ae6993c25b544c8ab6b5e0f00a896b2b1cc6bf7622a8cc51ac",
     "b911e2496a8f0bcb605f055d57dd369794a3bf90df4e1d0964ae39744a6bfaa0",
     "89a4a6c583d15c14956c8a75df9d149314cd3e71241b1c1ab6d3cc4aac05fd6c",
     "f30ad76c70bf282d744db484eb077668833e2848ded61de93376245758cbaeff",
     "6a0c6321e0190be5c94fdb706dce3915c798365ec3343a18886caa38f8601f19"},
    {"block/pr-check/space-and-an-edge.mmd", 95,
     "acdfea8e469258791ed57a2a9f9c0e914379f33102dcc29f4b579beaf4206c26",
     "640c0d4829b1a89b085cc392f1583539b1844df8495bf1739ec4bda6c8befff4",
     "1e22498d75610855245103b87675976f689a673a486a11840444f5f0c56e3622",
     "15862c1dba392803e18b642a88ce9508cc7a5168b3d338c97c148d5a31a16737",
     "0ab93e320b36d99371e1d3996f956435d4bbb2622882a4b834d98d0413ad7c7d",
     "c286c4f358118740ad237dd723a3cb7d3f1b7f214c4d08c24fbd0d25f89c35b0"},
    {"block/visual-check/composites-redux-color-theme.mmd", 96,
     "4e175cc997896e1d7536dc1a0b4dd189881d070b33499e71a0ae02124baddf82",
     "4b9248296861efb1c729523f5057d395656b2a7cf6bdc8d51efb33586341176b",
     "088492a37f6a7cbd3bdf90902a30c2be7aaae90e5b6c4caaa9423573b5073c02",
     "1f26a1675e1ebaab69d2cd3f66559cd4d97bc29d97fc3b3a9e52ee7460f0a02d",
     "f9e15ecea6e277cb3d291b33bb4721695e93182b478ca4836e78ac26537124c3",
     "828b06bd70e0ce3dc05aad93c9447779ed795837f10bc29b16cc147514443ef6"},
    {"block/visual-check/composites-redux-dark-color-theme.mmd", 97,
     "df6fca09147874107d251bc262bd79d3491a151b2b75c0e14089583bf80ea93c",
     "95d7661adb320994cfd55a68d84de37328647c335e184e61207732a2d96f8890",
     "6e5f90d83d9aab36c9b455eb25b9a081db1d2239c21ea84b657ffcd8868b8b48",
     "e82683330a6d8474a18540626bd18e3c6e807d6bc1afd5dcaace0a70e77eefb5",
     "da76ac1609871027dc1db8b540adb36a7db893de27c12a4024f844a13b48cb54",
     "e35794fcbaaae61ab10284eb715c27e64b60d0717acc580b38488d9b9a5f567a"},
    {"block/visual-check/composites-redux-dark-theme.mmd", 98,
     "fc6cda84b080f3587beeb1ef3ab1995f133895cdd98fbdfd7a08768549abcbc1",
     "f838172ca0a6da7663761cf6d3e15cd52ec5da9b131b303d62b0fc2bb47e85d1",
     "b968d0d571366cb3ede49776df5abeef445b651e6bf946858147516abb7384a8",
     "ea5eabdadd4dd88ebb8a190adf83980710869b46f9548bac6ee75fcbc0ee2abd",
     "6cf4029239e90dfc08cf1d2ceeeafc645e7c1f29e62675cb47d0823673407039",
     "984e19c8f399c5652365108d977fb72ac6fd10cfac8e9f26e9205e8a22713953"},
    {"block/visual-check/composites-redux-theme.mmd", 99,
     "17d1afcd5981c226de2c0ad0944988dab75f2f8b0c30d190aeb4e09c19ba9da3",
     "c36eec0b0fe2fb8539a2bdd57943aa97af58fa490c46cb367475a28dd0114778",
     "5e0fb9908f3b1c3991d7d1914e1e84606ec8a0a596777d69cb57cb4e34d91f3e",
     "e267741096011feb84520c87a7fe4c2161d2c53c5867b79651789d824cb0c91f",
     "362aef049974d69be9468e48dd6f74224df3c157857b96414ad656c485887c4b",
     "54793334e936d3dca4820f602f888415edb02ea568fe7d097d7727d4c823b6ed"},
    {"block/visual-check/flat-redux-color-theme.mmd", 100,
     "1bbb1a8933f9a53348736c6fcfb139e43128d63d1be66173c79f564e950141c5",
     "bb1b98c8a851228fdb620a4694b9d18830cd25ada1fe1e337cf4ea51a63f6ee7",
     "590e10570eed82ad518a4abbf33b1d61acf1fd4c4412702070ee0697b2de8f5e",
     "b79f6d22eb29270becb4d1b7a769444fc89870feecced5b90c6b9afb5e70c2f2",
     "d9e2720096fb4e86c5f74120f9e7c1cd141546fe0d506079caf7b9276392ce72",
     "65a0407100edb3541e362dc7478f38ef12e9d7a3dcd07183f87b29f0507df99f"},
    {"block/visual-check/flat-redux-dark-color-theme.mmd", 101,
     "9759628e675bb1f92f5e1dea086e259fefd8a1bbdd9e42dd82c123aeae63f6cd",
     "d0085f007f4ed9c39d4013d8c0b6a8360410a09a35122116f8a60682335dc4ee",
     "0471341bd31d32abb75e3e2921dc635ce3bf0a72556ea0e17256004722473d3b",
     "8b713ccf6eb5b9f754315be34c271b0188fa33d25b335c7b858722fda358a77e",
     "0c398bc231033a3f00a47345bb22744e3a9b6aaa0fc763ac21f2ed0ad002b7af",
     "7a1735bdc0dfdfca00b853987b63d1aa3f6c9386f25c9119ddfc22e59da7346a"},
    {"block/visual-check/flat-redux-dark-theme.mmd", 102,
     "04c2e44a8f08703458dc666d044dc80460717dbe29a0b397c42391ddd231db66",
     "be053c216019cc0869600c3e46e1b85631233f4c0cdd1e85463b7f26d7d7701e",
     "b2f7226ad406a7fa508473b500f5f5865864e8aba1e0d1b779259c2217ee9373",
     "157f824121284e497654045c472f37d2b59bee741496560c10b182e3855a191d",
     "349f6378a72ca5055168e6a5dfead7320e8dc51b83c6ade2e7888e573ce6b885",
     "aa19dc0956f084d6d5f874f45fb19b0b3a0c3f6525ed8cb8f952f4cb33e5c5fe"},
    {"block/visual-check/flat-redux-theme.mmd", 103,
     "19e9d75529a885c46aa6cfabefcce9cdaefa0fb7ba5a5a3958d1f1500da553b9",
     "bc35355c5090888da3aa0484ccc1d560f8e54a3e12cad80481ad173a3d038719",
     "0d4c67b82c34ced6b2a45e4b463d1de00ea77f0489d0ff4960403ef7acfdf9e9",
     "7295ef0297cbc9480849a38970ecbcb6f25e86901e7acd245f4c0c613ba7a7aa",
     "37e05e7cb861dbc760a2c772e20fc3ed7021253a70d9770e786cc17c4b2b97a3",
     "2e6059885ab31afebd1b8b301644b9e2b0bc2a8843ff0b75fbaae35ce3a3acdf"},
    {"block/visual-check/nested-redux-color-theme.mmd", 104,
     "98059c81a4ca97053c1e3f4e4601dba48145f76cc75855f30f8b92a90756d88f",
     "87d865463b2fb0e79367100326f72ac6f65a5bbb0ca94f855f18da41144470e5",
     "89682694eaa2d7972898b57d0a51cf703dc3e4a6ec94d43bb9dd9df9dfe7004e",
     "2b6d50be2a40b2966441db6e83b3838345daa71706701caaef42542776f928d5",
     "b5505533577fe7872b76d099cf7308589b5843b239bde57fe052a55f667ac3c3",
     "155bb1a0fca0d5f50a34c7113182ff9baf3bc9090d6fc9cf69c13267e9a13024"},
    {"block/visual-check/nested-redux-dark-color-theme.mmd", 105,
     "239b7a46e5ebd8f8aadda30b36f8e82d4926ff2904b6c9567157915e30b97282",
     "2f4a92a287fa8441dfb68fa167d84d47ada2c92e369f0f067a5c90b14953eb09",
     "055b8e9a6fabf22030276aae7b88d609641200dcacf5f3b5cd69b598c6f4e7b1",
     "80cceca2876d2e72a5c4d5dd34acc5068751060f9892f5a0d4deb675839cae91",
     "f9106016383dfe097a2205f4e84aeb5164f3bfa83b5f4cf8e35982378e738af0",
     "3895cc66cf0292ccdf732d914c10433bb5182e4385c3c6b81db07da8659b0db0"},
    {"block/visual-check/nested-redux-dark-theme.mmd", 106,
     "ec620eafadb4ccacf2cd5f843faf226a25716921b02b4b8fd72d97e817e1a3fa",
     "5ecaac2c8f19f175a2e4fd3346a1858fe04967b556ca431daab5c3c8309e4a40",
     "7d51a5bb2a22d288d6dc3df8881b94fe482fa7843dd7c6ceb6641a36d1b9b67c",
     "4594fd74998c6ecd7e3fce1c5fe2c96e14f4bb0ace4180d0ce96b913c189c92d",
     "3c2305ae260e208cce6ade8389306e42024991149c3bf83e83aea8a31f2d17e7",
     "c45f9a7a0cb2d7b4685b8387cf2f59641b21e4e9d13d2cc80c8d37e688269343"},
    {"block/visual-check/nested-redux-theme.mmd", 107,
     "fa00b314c98de9fa017d8b95ad99eda544656f50d3101b209d79e85a58badd5a",
     "857811b3687a461d7269eb2e4a48d31bfbfb000975cf350a47447c5036fd46be",
     "8454ab1d784370bb1dbe7c4ddeb1a38b55702a6ab2bb559bcb9513927cd2cfc6",
     "206a2d17132f4d075277f828a0c71ceebd3da7c50d592e54850b9280cc4f7a52",
     "39aa17c77d2365ec9afe1cb042ed2b7d15a0f17cbe2bd319dc25f9ff06fb9e17",
     "4a43b01f409f31ea581e93222bea926012ae115e1a3b9a68f1554408c822f23d"},
    {"block/visual-check/shapes-inside-a-container-redux-color-theme.mmd", 108,
     "6b7fd4e74add06384db3a4ece160e28870a062bb81af4cd32e795a24b131e0bf",
     "0d64accb5011b09bdd6be31743ffe10cd73f3120eedd43808a55a8ec046191bc",
     "5e14520a6690118ef0ab9dfe460c32742ba356fe1d5263867ade1225f5b5ae5a",
     "eebfea886691438987988f3c2376c0a69da81a769cc0e908c84e4460c8e38de7",
     "718ab8f33ecfb9645073d576e10d671d1664f4be5b8cdc9f66119f22ab9c0f09",
     "67f2f885af28efbd04d9f866ff135776360a97856b194dbc3273222d6ec709b1"},
    {"block/visual-check/shapes-inside-a-container-redux-dark-color-theme.mmd", 109,
     "ff50e858a0ff7baa29a42e918389065bf5a598a5f60e4e8656d89c7c9ee3ded7",
     "9a7defa4bcb735f5e79b81fe8b9403399859fc89032395a8da26f21759949518",
     "0fcd6a1370fd792ddbcf2df349c75d51831e9f7a9920903c71e617803d9cd4c1",
     "89f0217694948eff642d48648fe290885b213d88340618dbadabde04b1172e15",
     "ff2217164837b5df0bbf6597f5f96166c3f83c1f20da8e21aa47a5cb3c5224ab",
     "30938a6263e485cd42fb9d2bb04fe8556e6de444e2ca0db6a0def4ab2a40d516"},
    {"block/visual-check/shapes-inside-a-container-redux-dark-theme.mmd", 110,
     "7f9fc502fe257366fd27c7a25d7c1232560d4d2a4e025ca284d5615d7fb7d8b5",
     "2c2f1502be7aa4a6f85b99ad3200e56e7b0325a3532d7d4665a7696325c67ac8",
     "e65e33ea547f1e43ecf212a65e8a6a5af85e6fffd510d99efcf9be2162d32408",
     "cda94b56afe3cc0544b4e7e36e56f189957135d0d1e42c3bf7b6d1fc1be52613",
     "5e1457ed22b177e592300add28d752329b1f9e09533712e65bc160014b5acd4d",
     "b4ae510494db5e3ea473313fecca69c3189b9f70be25f80322d38ef54d604761"},
    {"block/visual-check/shapes-inside-a-container-redux-theme.mmd", 111,
     "998b179f9bd0832e83d067b15faf6273d61084e2f3d493c5a610ca817f700c97",
     "68386de258cb81dcb78c24e56d816007b49836be635700cb0c027596bff4b23f",
     "b88fc9eef8f655acf3b0e50c6ef62be393cf7ad5492c89df149a9b86cba5adcb",
     "57092bda6818b44234ef09131fbb2b9277b1840375e05e5a4eddee28e65d5d59",
     "5d58977724e5962ac4bf754b2f9039b9e3e9081e4c989de7614ff1e325db231e",
     "53c42a15e3062e40463b20b009ab3f189ad5f3ea67d16f8677880bb0eb7bdfd7"},
    {"block/visual-check/user-styled-redux-color-theme.mmd", 112,
     "95b815384562e5cf5fd3446e6c70a039dc60c0bcde944dc2c0c7b1d57c40363d",
     "8b6be4a1de1cd4a47c12d8312e0b57b082e60a1af23030d133b2f59c88d21234",
     "b32d3c1c6638d31f4432a3e4df40081458d50b4d97b0ddf0c19cab4f6d07dbcc",
     "42f19c68a663df35b45413002704f317d0082bbaba6c29344bdcefc93e75f2d5",
     "33ece8fbdcb1aca0ab5f0cf2c595426125565def88b13e635c1d7375b69db7a3",
     "62e56c0b4e1a172479e36288fb28aa5256fd554760b08ebc708eac652bc27037"},
    {"block/visual-check/user-styled-redux-dark-color-theme.mmd", 113,
     "858efa2929db709f3aba383e79fcbce178ba0b96cfb55536c0428ffbe5c95af7",
     "dbca462f88e22f74a5cadfd98c823b64b65703bcd0e837b40b8d0b11836d5b5f",
     "fa9c9e8dc05337f3e0976b7c7f460310becdb6aee2623099c0100539218e6d7a",
     "9a23448cabf5c4f441a5bb1f9b4a6a967189b671c68c5421e5d4cfde5ba1c63e",
     "a839799b2c9c7a7b1f0a9b7cae63ae763d839900bf2010a3d4cd6303b22e1374",
     "718d1c28d106716bdc5b6e6b1159c862aaa77d0a83b3e427dc9c3717e2cb6d9f"},
    {"block/visual-check/user-styled-redux-dark-theme.mmd", 114,
     "a7f080a4ffe6a1df3924b8b2b57097f6d1d003bfc0b665c76538224678102bc4",
     "75d841f2c2e4dd64f4479bc27ce315b6f7c75d7b568fc04cc55e539f41911cc9",
     "ec76d9a874ae8e81ede6aff88d6063ecf4b6508f9fd792c2f1bca6914246b05d",
     "2cb09157e84fd93a5e41ae07942dce831771c06d34a6ce51163c509a85709017",
     "b34be8027572d3b7ede97785a4b8f70f1d288f755dbf6763c9a0c69b0db04ccb",
     "c603344e2eddb5054340fa372f14c1a68108fc264a132d0ee3933e1aa8c84bc8"},
    {"block/visual-check/user-styled-redux-theme.mmd", 115,
     "2c9b6972c033a74bf0980993dc4f6813b163643e285bbb5aa3445fc58d7554e9",
     "91840b7491cdf0574564524e9a2ae193b7ea382bfeb3a2211dc45088de5092da",
     "0787818ed1cdc9ff180e77e5e1f4a8b00a9a7c63626264dd86ca77ec5de71c7f",
     "00ca18f32be84203baeafd2cd968b58abdfb003eeb2b91273439461d8c0fe87c",
     "99da3e5c308fd51471bcb9d05704760fa5368c39c83d68c89dce6d1702358632",
     "48e09bc2021591e4fd11c3a856c6334a0b4848b77bb7e43489549276b9cc127d"},
}};

namespace detail {
inline constexpr std::string_view kSourceCanonicalSha =
    "ffa3d505b46cf00268d4ded5e3b6e290d7d809a05ec99e5c5aae9b303735f93a";
inline constexpr std::string_view kTerminalCanonicalSha =
    "d67bc429b16bfcd03f9b9b8bebb2c6c939912d289b6e30cf954179a3bbfc5d89";
inline constexpr std::string_view kContractSha =
    "9c8c2b608640c284e0a4ea6332f1e0616f6f71309b2cb5ec13e55ab90920ebda";

inline void need(bool ok, std::string_view message) {
    if (!ok) throw std::runtime_error(std::string(message));
}

inline void pay(OracleWork& w, std::size_t n, std::size_t scale, std::size_t fixed = 0) {
    if (fixed > w.left || (scale && n > (w.left - fixed) / scale))
        throw std::runtime_error("resource: Block58 checked Work charge");
    w.spend(fixed + n * scale);
}

inline JsonVal get(JsonVal obj, std::string_view key, OracleWork& w) {
    need(obj.is_obj() && obj.size() <= 65536, "protocol: Block58 object lookup");
    pay(w, obj.size(), 8, 64);
    return obj.get(key);
}

inline JsonVal at(JsonVal array, std::size_t index, OracleWork& w) {
    need(array.is_arr() && array.size() <= 65536 && index < array.size(),
         "protocol: Block58 array index");
    pay(w, index, 1, 8);
    return array.at(index);
}

inline std::string_view text(JsonVal v, OracleWork& w, std::size_t limit = 65536) {
    w.spend(8);
    need(v.valid() && v.is_str(), "protocol: Block58 string type");
    const auto s = v.as_str();
    need(s.size() <= limit, "resource: Block58 string byte bound");
    pay(w, s.size(), 1);
    return s;
}

inline void same_text(JsonVal v, std::string_view expected, OracleWork& w, std::string_view why) {
    const auto s = text(v, w);
    pay(w, std::min(s.size(), expected.size()), 1, 1);
    need(s == expected, why);
}

inline std::int64_t integer(JsonVal v, OracleWork& w) {
    w.spend(128);
    need(v.valid() && v.is_int(), "protocol: Block58 integer type");
    const auto n = v.as_int();
    need(n >= -1 && n <= 1'000'000, "protocol: Block58 integer range");
    // An unsigned JSON value must not wrap through as_int() into a frozen -1.
    std::array<char, 32> buf{};
    const auto [end, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), n);
    need(ec == std::errc{}, "protocol: Block58 integer encoding");
    need(v.to_string() == std::string_view(buf.data(), static_cast<std::size_t>(end - buf.data())),
         "protocol: Block58 integer signedness/representation");
    return n;
}

inline void zero(JsonVal v, OracleWork& w) {
    need(integer(v, w) == 0, "source: Block58 zero credit required");
}

inline void keys(JsonVal obj, std::initializer_list<std::string_view> wanted, OracleWork& w) {
    w.spend(128);
    need(obj.is_obj() && wanted.size() <= 64 && obj.size() == wanted.size(),
         "protocol: Block58 exact object keys");
    std::array<bool, 64> seen{};
    std::size_t count = 0;
    obj.iter_obj([&](JsonVal k, JsonVal) {
        const auto name = text(k, w, 256);
        auto found = wanted.begin();
        for (; found != wanted.end(); ++found) {
            pay(w, std::min(name.size(), found->size()), 1, 1);
            if (name == *found) break;
        }
        need(found != wanted.end(), "protocol: Block58 unknown field");
        const auto i = static_cast<std::size_t>(found - wanted.begin());
        need(!seen[i], "protocol: Block58 duplicate field");
        seen[i] = true;
        ++count;
    });
    need(count == wanted.size(), "protocol: Block58 missing field");
}

inline std::string digest(std::string_view bytes, OracleWork& w) {
    pay(w, bytes.size(), 8, 256);
    return loom::utils::crypto::sha256(bytes);
}

// Validate scalar UTF-8 before preserving its bytes. No replacement, Unicode
// normalization or ASCII-only policy is imposed on the source authority.
inline std::size_t quoted_size(std::string_view s, OracleWork& w) {
    need(s.size() <= 65536, "resource: Block58 canonical string bound");
    pay(w, s.size(), 3, 8);
    std::size_t result = 2;
    for (std::size_t i = 0; i < s.size();) {
        const auto lead = static_cast<unsigned char>(s[i]);
        std::size_t n = 1;
        std::uint32_t cp = lead;
        if (lead >= 0xc2 && lead <= 0xdf) {
            n = 2;
            cp = lead & 31;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            n = 3;
            cp = lead & 15;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            n = 4;
            cp = lead & 7;
        } else
            need(lead < 128, "protocol: Block58 invalid UTF-8 lead");
        need(n <= s.size() - i, "protocol: Block58 truncated UTF-8");
        for (std::size_t j = 1; j < n; ++j) {
            const auto c = static_cast<unsigned char>(s[i + j]);
            need((c & 0xc0) == 0x80, "protocol: Block58 invalid UTF-8 continuation");
            cp = (cp << 6) | (c & 63);
        }
        need((n == 1 || cp >= (n == 2   ? 0x80u
                               : n == 3 ? 0x800u
                                        : 0x10000u)) &&
                 cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff),
             "protocol: Block58 invalid UTF-8 scalar");
        if (n != 1)
            result += n;
        else if (lead == '"' || lead == '\\' || lead == '\b' || lead == '\f' || lead == '\n' ||
                 lead == '\r' || lead == '\t')
            result += 2;
        else
            result += lead < 32 ? 6 : 1;
        i += n;
    }
    return result;
}
enum class RealScope { none, terminal_root, terminal_model };

struct Step {
    std::string_view key;
    std::size_t index = 0;
    bool array = false;
};

struct Encoder {
    OracleWork& work;
    RealScope scope;
    std::array<Step, 64> path{};
    std::size_t containers = 0, values = 0, expected = 0;

    static void add(std::size_t& total, std::size_t n) {
        need(total <= kMaxFactBytes && n <= kMaxFactBytes - total,
             "resource: Block58 canonical byte bound");
        total += n;
    }

    bool named(std::size_t i, std::string_view name) const {
        return !path[i].array && path[i].key == name;
    }

    bool indexed(std::size_t i, std::size_t n) const { return path[i].array && path[i].index == n; }

    std::string_view real_token(JsonVal v, JsonVal parent, std::size_t depth) {
        need(v.is_double() && !v.is_int(), "protocol: Block58 real type");
        std::size_t p = 0;
        if (scope == RealScope::terminal_root) {
            need(depth == 9 && named(0, "cases") && indexed(1, 22) &&
                     named(2, "proposed_terminal_model"),
                 "protocol: Block58 real outside frozen terminal case");
            p = 3;
        } else
            need(scope == RealScope::terminal_model && depth == 6,
                 "protocol: Block58 real outside terminal model");
        need(named(p, "styles") && path[p + 1].array && path[p + 1].index <= 1 &&
                 named(p + 4, "value") && named(p + 5, "width"),
             "protocol: Block58 real outside width field");
        need((named(p + 2, "ordered_property_atoms") && indexed(p + 3, 2)) ||
                 (named(p + 2, "resolved_authored_properties") && named(p + 3, "stroke-width")),
             "protocol: Block58 real outside frozen style path");
        keys(parent, {"source_literal", "width", "border_emphasis", "glyph_substitution", "status"},
             work);
        const bool two = path[p + 1].index == 0;
        same_text(get(parent, "source_literal", work), two ? "2px" : "4px", work,
                  "source: Block58 real/source literal pair");
        need(v.as_double() == (two ? 2.0 : 4.0), "source: Block58 only frozen 2.0/4.0 widths");
        return two ? "2.0" : "4.0";
    }

    std::size_t measure(JsonVal v, std::size_t depth = 0, JsonVal parent = JsonVal{}) {
        work.spend(64);
        need(v.valid(), "protocol: Block58 invalid canonical value");
        need(depth < 64 && ++values <= 262144, "resource: Block58 canonical depth/value bound");
        if (v.is_str()) return quoted_size(v.as_str(), work);
        if (v.is_int()) {
            const auto n = integer(v, work);
            std::array<char, 32> b{};
            const auto [e, ec] = std::to_chars(b.data(), b.data() + b.size(), n);
            need(ec == std::errc{}, "protocol: Block58 integer encoding");
            return static_cast<std::size_t>(e - b.data());
        }
        if (v.is_double()) return real_token(v, parent, depth).size();
        if (v.is_bool()) return v.as_bool() ? 4 : 5;
        if (v.is_null()) return 4;
        need(v.is_obj() || v.is_arr(), "protocol: Block58 canonical type");
        need(++containers <= 65536 && v.size() <= 65536,
             "resource: Block58 canonical container bound");
        std::size_t n = 2, count = 0;
        if (v.is_arr())
            v.iter([&](JsonVal child) {
                if (count) add(n, 1);
                path[depth] = {{}, count, true};
                ++count;
                add(n, measure(child, depth + 1, v));
            });
        else
            v.iter_obj([&](JsonVal k, JsonVal child) {
                const auto name = text(k, work, 256);
                if (count++) add(n, 1);
                add(n, quoted_size(name, work));
                add(n, 1);
                path[depth] = {name, 0, false};
                add(n, measure(child, depth + 1, v));
            });
        need(count == v.size(), "protocol: Block58 inconsistent container cardinality");
        return n;
    }

    void put(std::string& out, std::string_view bytes) {
        need(out.size() <= expected && bytes.size() <= expected - out.size(),
             "resource: Block58 canonical output bound");
        pay(work, bytes.size(), 1);
        out.append(bytes);
    }

    void quote(std::string& out, std::string_view s) {
        put(out, "\"");
        constexpr std::string_view hex = "0123456789abcdef";
        for (const unsigned char c : s) {
            work.spend(1);
            switch (c) {
            case '"':
                put(out, "\\\"");
                break;
            case '\\':
                put(out, "\\\\");
                break;
            case '\b':
                put(out, "\\b");
                break;
            case '\f':
                put(out, "\\f");
                break;
            case '\n':
                put(out, "\\n");
                break;
            case '\r':
                put(out, "\\r");
                break;
            case '\t':
                put(out, "\\t");
                break;
            default:
                if (c < 32) {
                    const std::array<char, 6> esc{'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
                    put(out, {esc.data(), esc.size()});
                } else {
                    const char byte = static_cast<char>(c);
                    put(out, {&byte, 1});
                }
            }
        }
        put(out, "\"");
    }

    void emit(JsonVal v, std::string& out, std::size_t depth = 0, JsonVal parent = JsonVal{}) {
        work.spend(64);
        need(depth < 64, "resource: Block58 canonical emission depth");
        if (v.is_str()) {
            quote(out, v.as_str());
            return;
        }
        if (v.is_int()) {
            // measure() already checked range and unsigned representation on the
            // same immutable view. Emit directly without a second JSON stringify.
            work.spend(8);
            const auto n = v.as_int();
            std::array<char, 32> b{};
            const auto [e, ec] = std::to_chars(b.data(), b.data() + b.size(), n);
            need(ec == std::errc{}, "protocol: Block58 integer encoding");
            put(out, {b.data(), static_cast<std::size_t>(e - b.data())});
            return;
        }
        if (v.is_double()) {
            put(out, real_token(v, parent, depth));
            return;
        }
        if (v.is_bool()) {
            put(out, v.as_bool() ? "true" : "false");
            return;
        }
        if (v.is_null()) {
            put(out, "null");
            return;
        }
        if (v.is_arr()) {
            put(out, "[");
            std::size_t i = 0;
            v.iter([&](JsonVal child) {
                if (i) put(out, ",");
                path[depth] = {{}, i++, true};
                emit(child, out, depth + 1, v);
            });
            put(out, "]");
            return;
        }

        struct Entry {
            std::string_view name;
            JsonVal value;
        };

        pay(work, v.size(), 2 * sizeof(Entry) + 64, 64);
        std::vector<Entry> entries;
        entries.reserve(v.size());
        v.iter_obj(
            [&](JsonVal key, JsonVal child) { entries.push_back({text(key, work, 256), child}); });
        std::ranges::sort(entries, [&](const Entry& a, const Entry& b) {
            pay(work, std::min(a.name.size(), b.name.size()), 1, 1);
            return a.name < b.name;
        });
        put(out, "{");
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            if (i) {
                pay(work, std::min(e.name.size(), entries[i - 1].name.size()), 1, 1);
                need(e.name != entries[i - 1].name, "protocol: Block58 canonical duplicate key");
                put(out, ",");
            }
            quote(out, e.name);
            put(out, ":");
            path[depth] = {e.name, 0, false};
            emit(e.value, out, depth + 1, v);
        }
        put(out, "}");
    }
};

inline std::string canonical(JsonVal v, OracleWork& w, RealScope scope = RealScope::none) {
    Encoder e{w, scope};
    e.expected = e.measure(v);
    // Charge both the retained output and conservative allocation rounding before reserve.
    pay(w, e.expected, 2, 64);
    std::string out;
    out.reserve(e.expected);
    e.emit(v, out);
    need(out.size() == e.expected, "protocol: Block58 canonical measured/encoded size");
    return out;
}

inline void pin(JsonVal v, std::string_view expected, OracleWork& w,
                RealScope scope = RealScope::none) {
    const auto bytes = canonical(v, w, scope);
    need(digest(bytes, w) == expected, "source: Block58 frozen canonical component pin");
}

inline void source_case_keys(JsonVal v, OracleWork& w) {
    keys(v,
         {"admission_status",
          "body_span_witness",
          "case_relative_path",
          "complete_model",
          "complete_model_sha256",
          "counts",
          "drawing_credit",
          "full_source",
          "full_source_witness",
          "manifest_global_ordinal",
          "manifest_record",
          "manifest_record_sha256",
          "path",
          "physical_line_witnesses",
          "proposed_cohorts",
          "review_status",
          "source_bytes",
          "source_git_blob",
          "source_sha256",
          "source_witnesses",
          "source_witnesses_sha256",
          "terminal_projection",
          "terminal_projection_sha256",
          "terminal_projection_status",
          "upstream_assertion_credit"},
         w);
}

inline void terminal_case_keys(JsonVal v, OracleWork& w) {
    keys(v,
         {"B1_status", "B2_status", "drawing_credit", "path", "proposed_terminal_model",
          "proposed_terminal_model_sha256", "runtime_admission", "source_binding",
          "source_binding_sha256"},
         w);
}

inline void witness_slices(JsonVal source_case, std::string_view raw, OracleWork& w) {
    const auto ws = get(source_case, "source_witnesses", w);
    need(ws.is_arr() && ws.size() > 0 && ws.size() <= 4096,
         "protocol: Block58 witness array bound");
    for (std::size_t i = 0; i < ws.size(); ++i) {
        const auto x = at(ws, i, w);
        keys(x,
             {"ordinal", "role", "source_byte_end", "source_byte_start", "source_lexeme",
              "source_line_end", "source_line_start"},
             w);
        need(integer(get(x, "ordinal", w), w) == static_cast<std::int64_t>(i),
             "source: Block58 witness ordinal");
        const auto a = integer(get(x, "source_byte_start", w), w),
                   b = integer(get(x, "source_byte_end", w), w);
        need(a >= 0 && b >= a && static_cast<std::uint64_t>(b) <= raw.size(),
             "source: Block58 witness byte range");
        const auto begin = static_cast<std::size_t>(a), end = static_cast<std::size_t>(b);
        same_text(get(x, "source_lexeme", w), raw.substr(begin, end - begin), w,
                  "source: Block58 exact witness slice");
        pay(w, begin, 1, 1);
        const auto start_line = 1 + std::ranges::count(raw.substr(0, begin), '\n');
        const auto last = std::max(begin, end ? end - 1 : 0);
        pay(w, last, 1, 1);
        const auto end_line = 1 + std::ranges::count(raw.substr(0, last), '\n');
        need(integer(get(x, "source_line_start", w), w) == start_line &&
                 integer(get(x, "source_line_end", w), w) == end_line,
             "source: Block58 witness line range");
    }
    const auto full = integer(get(source_case, "full_source_witness", w), w);
    need(full >= 0, "source: Block58 full-source witness index");
    const auto x = at(ws, static_cast<std::size_t>(full), w);
    need(integer(get(x, "source_byte_start", w), w) == 0 &&
             integer(get(x, "source_byte_end", w), w) == static_cast<std::int64_t>(raw.size()),
         "source: Block58 full-source witness extent");
}
}  // namespace detail

// Run before caller parsing. There is no hidden cache or mutable process state.
inline void verify_asset_bytes(std::string_view sourceBytes, std::string_view terminalBytes,
                               OracleWork& w) {
    using namespace detail;
    try {
        need(sourceBytes.size() <= kMaxFactBytes && terminalBytes.size() <= kMaxFactBytes,
             "resource: Block58 factfile exceeds4MiB");
        need(digest(sourceBytes, w) == kSourceFactsSha, "source: Block58 source asset byte SHA");
        need(digest(terminalBytes, w) == kTerminalFactsSha,
             "source: Block58 terminal asset byte SHA");
    } catch (const std::bad_alloc&) {
        throw std::runtime_error("resource: Block58 asset hash allocation failure");
    }
}

inline BoundFacts bind(JsonVal sourceRoot, JsonVal terminalRoot, std::string_view path,
                       std::string_view rawSource, JsonVal manifestRoot, OracleWork& w) {
    using namespace detail;
    try {
        need(rawSource.size() <= kMaxRawBytes, "resource: Block58 raw source exceeds64KiB");
        need(path.size() <= 1024, "resource: Block58 fixture path bound");
        // Freeze both fact roots independently as well: skipping verify_asset_bytes
        // cannot turn recomputed component hashes or changed root metadata into trust.
        // This also rejects duplicate/unknown nested fields without duplicating the
        // separate projection/geometry schema checker owned by the caller.
        pin(sourceRoot, kSourceCanonicalSha, w);
        pin(terminalRoot, kTerminalCanonicalSha, w, RealScope::terminal_root);
        keys(sourceRoot,
             {"authority",
              "canonical_encoding",
              "cases",
              "counts",
              "hash_field_mapping",
              "planning_context_user_supplied",
              "proposed_cohorts",
              "qualification",
              "review",
              "review_status",
              "schema",
              "scope",
              "self_check",
              "semantic_rules",
              "semantic_uncertainties",
              "source_lock_encoding",
              "source_lock_sha256",
              "source_palette_authority",
              "terminal_projection",
              "terminal_projection_status"},
             w);
        keys(terminalRoot,
             {"additional_evidence",
              "blocking_unknowns",
              "canonical_encoding",
              "caption_policy",
              "cases",
              "counts",
              "current_rfc_contract_bindings",
              "entity_witnesses",
              "ownership",
              "palette_projection_candidates",
              "palette_projection_candidates_sha256",
              "qualification",
              "review",
              "rfc_basis",
              "schema",
              "schema_contract",
              "self_check",
              "shape_rules",
              "source_authority",
              "source_evidence_refs",
              "source_uncertainty_catalog",
              "source_uncertainty_disposition",
              "status",
              "style_policy"},
             w);
        keys(manifestRoot, {"schema", "upstream", "tracks", "oracle_basis", "fixtures"}, w);
        const auto scases = get(sourceRoot, "cases", w), tcases = get(terminalRoot, "cases", w);
        need(scases.is_arr() && tcases.is_arr() && scases.size() == kPins.size() &&
                 tcases.size() == kPins.size(),
             "protocol: Block58 complete58 case cardinality");
        std::optional<std::size_t> selected;
        std::size_t lock_size = 0;
        for (const auto& p : kPins) {
            pay(w, p.path.size(), 1, 8);
            need(p.path.starts_with("block/"), "protocol: Block58 compiled pin path");
            lock_size += p.path.size() - 6 + 66;
        }
        pay(w, lock_size, 2, 64);
        std::string lock;
        lock.reserve(lock_size);
        for (std::size_t i = 0; i < kPins.size(); ++i) {
            const auto& p = kPins[i];
            const auto s = at(scases, i, w), t = at(tcases, i, w);
            source_case_keys(s, w);
            terminal_case_keys(t, w);
            same_text(get(s, "path", w), p.path, w,
                      "source: Block58 unique ordered source identities");
            same_text(get(t, "path", w), p.path, w,
                      "source: Block58 unique ordered terminal identities");
            need(integer(get(s, "manifest_global_ordinal", w), w) == p.global,
                 "source: Block58 pinned global ordinal");
            same_text(get(s, "case_relative_path", w), p.path.substr(6), w,
                      "source: Block58 relative lock path");
            for (const auto pair : {std::pair{"source_sha256", p.raw_sha},
                                    std::pair{"complete_model_sha256", p.source_model_sha},
                                    std::pair{"source_witnesses_sha256", p.witness_sha},
                                    std::pair{"manifest_record_sha256", p.manifest_record_sha}})
                same_text(get(s, pair.first, w), pair.second, w,
                          "source: Block58 immutable source component declaration");
            same_text(get(t, "proposed_terminal_model_sha256", w), p.terminal_model_sha, w,
                      "source: Block58 immutable terminal model declaration");
            same_text(get(t, "source_binding_sha256", w), p.terminal_source_binding_sha, w,
                      "source: Block58 immutable source-binding declaration");
            pay(w, p.path.size() + 66, 1);
            lock.append(p.path.substr(6));
            lock.push_back('\0');
            lock.append(p.raw_sha);
            lock.push_back('\n');
            pay(w, std::min(path.size(), p.path.size()), 1, 1);
            if (path == p.path) selected = i;
        }
        need(selected.has_value(), "source: Block58 unknown selected path");
        need(lock.size() == lock_size && digest(lock, w) == kSourceLock,
             "source: Block58 path/NUL/ASCII-SHA/LF lock");
        same_text(get(sourceRoot, "source_lock_sha256", w), kSourceLock, w,
                  "source: Block58 source lock declaration");
        same_text(get(get(sourceRoot, "authority", w), "official_manifest_sha256", w), kManifestSha,
                  w, "source: Block58 original manifest SHA declaration");
        const auto contracts = get(terminalRoot, "current_rfc_contract_bindings", w);
        pin(contracts, kContractSha, w);
        same_text(get(get(terminalRoot, "rfc_basis", w), "current_contract_bindings_sha256", w),
                  kContractSha, w,
                  "source: Block58 full current-contract array digest, never a subset");
        const auto& p = kPins[*selected];
        const auto s = at(scases, *selected, w), t = at(tcases, *selected, w);
        const auto sm = get(s, "complete_model", w), tm = get(t, "proposed_terminal_model", w);
        pin(sm, p.source_model_sha, w);
        pin(tm, p.terminal_model_sha, w, RealScope::terminal_model);
        pin(get(s, "source_witnesses", w), p.witness_sha, w);
        const auto records = get(manifestRoot, "fixtures", w);
        need(records.is_arr() && records.size() == 2145,
             "protocol: Block58 frozen manifest cardinality");
        const auto record = at(records, static_cast<std::size_t>(p.global), w);
        pin(record, p.manifest_record_sha, w);
        pin(get(s, "manifest_record", w), p.manifest_record_sha, w);
        same_text(get(record, "path", w), p.path, w, "source: Block58 selected manifest path");
        same_text(get(record, "sha256", w), p.raw_sha, w,
                  "source: Block58 manifest/raw SHA identity");
        same_text(get(s, "source_git_blob", w), text(get(record, "git_blob", w), w, 40), w,
                  "source: Block58 Git blob/frozen manifest identity");
        // The immutable manifest-record pin includes the exact Git blob. Raw SHA
        // is computed independently; no second, handwritten SHA-1 is introduced.
        need(digest(rawSource, w) == p.raw_sha, "source: Block58 selected raw bytes SHA");
        need(integer(get(s, "source_bytes", w), w) == static_cast<std::int64_t>(rawSource.size()) &&
                 integer(get(record, "bytes", w), w) == static_cast<std::int64_t>(rawSource.size()),
             "source: Block58 raw byte count");
        same_text(get(s, "full_source", w), rawSource, w,
                  "source: Block58 complete original source witness");
        witness_slices(s, rawSource, w);
        const auto binding = get(t, "source_binding", w);
        keys(binding,
             {"B1_reviewed_proposal_sha256", "complete_source_model_sha256",
              "current_source_asset_sha256", "manifest_global_ordinal", "manifest_record_sha256",
              "path", "source_asset_path", "source_bytes", "source_git_blob", "source_lock_sha256",
              "source_sha256", "source_witnesses_sha256"},
             w);
        pin(binding, p.terminal_source_binding_sha, w);
        same_text(get(binding, "path", w), p.path, w, "source: Block58 binding path");
        same_text(get(binding, "current_source_asset_sha256", w), kSourceFactsSha, w,
                  "source: Block58 binding current source asset");
        same_text(get(binding, "source_lock_sha256", w), kSourceLock, w,
                  "source: Block58 binding source lock");
        same_text(get(binding, "source_sha256", w), p.raw_sha, w,
                  "source: Block58 binding raw SHA");
        same_text(get(binding, "complete_source_model_sha256", w), p.source_model_sha, w,
                  "source: Block58 binding source model SHA");
        same_text(get(binding, "source_witnesses_sha256", w), p.witness_sha, w,
                  "source: Block58 binding witness SHA");
        same_text(get(binding, "manifest_record_sha256", w), p.manifest_record_sha, w,
                  "source: Block58 binding manifest record SHA");
        same_text(get(binding, "source_git_blob", w), text(get(record, "git_blob", w), w, 40), w,
                  "source: Block58 binding Git blob");
        need(integer(get(binding, "manifest_global_ordinal", w), w) == p.global &&
                 integer(get(binding, "source_bytes", w), w) ==
                     static_cast<std::int64_t>(rawSource.size()),
             "source: Block58 binding ordinal/bytes");
        zero(get(s, "drawing_credit", w), w);
        zero(get(s, "upstream_assertion_credit", w), w);
        zero(get(t, "drawing_credit", w), w);
        zero(get(tm, "drawing_credit", w), w);
        zero(get(tm, "PaletteFG_credit", w), w);
        zero(get(tm, "foreign_rgb_css_svg_assertion_credit", w), w);
        need(get(s, "terminal_projection", w).is_null() &&
                 get(s, "terminal_projection_sha256", w).is_null() &&
                 get(tm, "geometry", w).is_null() && get(tm, "terminal_style_planes", w).is_null(),
             "source: Block58 expected unimplemented/null geometry");
        const auto runtime = get(t, "runtime_admission", w);
        need(runtime.is_bool() && !runtime.as_bool(), "source: Block58 no runtime admission");
        // Nested UNREVIEWED statuses are intentionally retained by the immutable
        // source/projection facts. B1 source approval is not B2 or runtime approval.
        pay(w, path.size(), 2, 64);
        return {s, t, sm, tm, std::string(path), p.global};
    } catch (const std::bad_alloc&) {
        throw std::runtime_error("resource: Block58 oracle allocation failure");
    }
}
}  // namespace loom::test::mermaid_block58
