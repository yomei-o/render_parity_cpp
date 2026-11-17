#!/bin/sh
# ローカルの USE_MYSQL ビルド（TiDB/MySQL）を動かすための MySQL クライアント DLL を
# ビルド出力先(x64/Release, x64/Debug)にコピーする。毎回 DLL を手で探さなくて済むように。
#
# これらの DLL は .gitignore 済み（コミットされない・ローカル専用）。ライセンスは関係ない
# ＝自分のマシンで動かすだけ（配布でない）。
#
# 既定のコピー元は Oracle MySQL 8.4（ヘッダ mysql_8_4 と ABI 一致で確実に動く）。
# 別の場所なら:  MYSQL_DIR=/c/path/to/mysql  sh setup_local_dll.sh
set -e
MYSQL_DIR="${MYSQL_DIR:-/c/prog/mysql-8.4.4-winx64}"
HERE="$(cd "$(dirname "$0")" && pwd)"

if [ ! -f "$MYSQL_DIR/lib/libmysql.dll" ]; then
  echo "libmysql.dll が見つかりません: $MYSQL_DIR/lib/libmysql.dll" >&2
  echo "MYSQL_DIR で MySQL のインストール先を指定してください。" >&2
  exit 1
fi

for cfg in Release Debug; do
  dst="$HERE/x64/$cfg"
  mkdir -p "$dst"
  cp -f "$MYSQL_DIR/lib/libmysql.dll"        "$dst/" && echo "  libmysql.dll      -> x64/$cfg"
  cp -f "$MYSQL_DIR/bin/libssl-3-x64.dll"    "$dst/" 2>/dev/null && echo "  libssl-3-x64.dll  -> x64/$cfg" || true
  cp -f "$MYSQL_DIR/bin/libcrypto-3-x64.dll" "$dst/" 2>/dev/null && echo "  libcrypto-3-x64.dll -> x64/$cfg" || true
done
echo "done. USE_MYSQL ビルドの exe はこれで DLL を隣から読む（PATH 設定不要）。"
