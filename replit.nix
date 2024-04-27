{ pkgs }: {
	deps = [
   pkgs.python39Packages.clvm-tools
   pkgs.recode
		pkgs.clang_12
		pkgs.ccls
		pkgs.gdb
		pkgs.gnumake
	];
}