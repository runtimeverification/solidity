{ lib, gccStdenv, cleanGitSubtree, cleanSourceWith, fetchzip
, boost
, cmake
, coreutils
, fetchpatch
, ncurses
, python3
, z3Support ? true
, z3 ? null
, cvc4Support ? true
, cvc4 ? null
, cln ? null
, gmp ? null
, llvm
, kiele
}:

# compiling source/libsmtutil/CVC4Interface.cpp breaks on clang on Darwin,
# general commandline tests fail at abiencoderv2_no_warning/ on clang on NixOS

assert z3Support -> z3 != null && lib.versionAtLeast z3.version "4.6.0";
assert cvc4Support -> cvc4 != null && cln != null && gmp != null;

let
  jsoncppVersion = "1.9.3";
  jsoncppUrl = "https://github.com/open-source-parsers/jsoncpp/archive/${jsoncppVersion}.tar.gz";
  jsoncpp = fetchzip {
    url = jsoncppUrl;
    sha256 = "1vbhi503rgwarf275ajfdb8vpdcbn1f7917wjkf8jghqwb1c24lq";
  };

  isolc = gccStdenv.mkDerivation rec {
    pname = "isolc";
    version = "0.8.2";

    src =
      cleanSourceWith {
        name = "solidity";
        src = cleanGitSubtree { src = ./..; name = "solidity"; };
        ignore = [
          "nix/"
          "*.nix"
        ];
      };

    prePatch = ''
      echo "0000000000000000000000000000000000000000" >commit_hash.txt
    '';

    NIX_CFLAGS_COMPILE = "-Wno-error=maybe-uninitialized";

    postPatch = ''
      substituteInPlace cmake/jsoncpp.cmake \
        --replace "${jsoncppUrl}" ${jsoncpp}

      for i in ./scripts/*.sh ./scripts/*.py ./test/*.sh ./test/*.py; do
        patchShebangs "$i"
      done
    '';

    cmakeFlags = [
      "-DBoost_USE_STATIC_LIBS=OFF"
    ] ++ lib.optionals (!z3Support) [
      "-DUSE_Z3=OFF"
    ] ++ lib.optionals (!cvc4Support) [
      "-DUSE_CVC4=OFF"
    ];

    nativeBuildInputs = [ cmake ];
    buildInputs = [ boost llvm ]
      ++ lib.optionals z3Support [ z3 ]
      ++ lib.optionals cvc4Support [ cvc4 cln gmp ];

    doInstallCheck = true;
    installCheckPhase = ''
      $out/bin/isolc --version > /dev/null
    '';

    doCheck = true;
    checkInputs = [ kiele ncurses python3 ];
    checkPhase = ''
      (
        cd "$NIX_BUILD_TOP/$sourceRoot"
        ./test/ieleCmdlineTests.sh
      )
    '';

    meta = with lib; {
      description = "Compiler for Ethereum smart contract language Solidity";
      homepage = "https://github.com/ethereum/solidity";
      license = licenses.gpl3;
      platforms = with platforms; linux; # darwin is currently broken
      inherit version;
    };
  };
in

isolc
