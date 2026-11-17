# render_parity_cpp

パスキー（WebAuthn）サーバの軽量サンプルを、**同じ API を FastAPI(Python) と cpp-httplib(C++) の
両方**で実装し、**Render.com** にデプロイするための入れ物。DB は **TiDB Cloud（MySQL 互換）** を予定。

元サンプル: [yomei-o/passkey_server_cpp](https://github.com/yomei-o/passkey_server_cpp) の `src/server_sample`。
Python/C++ の**機能パリティ**を保つのが方針（速度は別）。

## 構成

```
render.yaml       Render Blueprint（python と cpp の2サービス）
cpp/              cpp-httplib 実装（httplib + 自作mbedtls + sqlpp11 + sqlite3 同梱）
  main/           lambda_server.cpp（エントリ）, API(testsv_api_*), httplib/json
  include/        mbedtls（mbedtlspp 経由でユニティ include）
  sqlite/ sqlpp11/ mysql_8_4/ postgre_17/ models/ gen/ date/
  html/           index.html, qr.js/qr.wasm（静的配信 /html）
  Dockerfile      Render 用（既定 SQLite ビルド。TiDB/MySQL への切替はコメント参照）
python/           FastAPI 実装
  lambda_server.py  app（passkey/tenant ルータを /api 配下、/html 配信、/ は redirect）
  database.py       既定 SQLite / env で MySQL(TiDB) / DATABASE_URL 優先
  requirements.txt
```

## 共通の約束

- **ポートは環境変数 `POST`**（PORT ではない。既定 8080）。Render は `PORT` を注入するので、
  Python は `uvicorn ... --port $PORT`、C++ は Docker CMD で `POST=$PORT` に橋渡しする。
- **DB は既定 SQLite**（`test_db.db`、テーブルは起動時に自動作成）。
  永続化は **TiDB Cloud**：env `RDS_PROXY_HOST / USER_NAME / PASSWORD / DB_NAME`（Python は `DATABASE_URL` も可）。
- CORS 全許可。`/html/index.html` は 200（Render のヘルスチェックに使用）。`/` は `/html/index.html` へ redirect。

## ローカルで動かす

**Python（FastAPI）**
```sh
cd python
python -m venv .venv && . .venv/Scripts/activate      # mac/Linux は .venv/bin/activate
pip install -r requirements.txt
POST=8080 python lambda_server.py                      # http://localhost:8080/html/index.html
```

**C++（cpp-httplib）**
- Windows: `cpp/sqlpp11_cust.sln` を VS2022 で Release ビルド → `POST=8080 server.exe`
- Linux/Docker:
  ```sh
  cd cpp && docker build -t parity-cpp . && docker run -e PORT=8080 -p 8080:8080 parity-cpp
  ```

## Render へのデプロイ

1. この repo を push（`render.yaml` はリポ直下）。
2. Render → **New + → Blueprint** → この repo を選択 → 2サービス作成。
   - python … ネイティブ Python（`pip install` → `uvicorn`）
   - cpp … `cpp/Dockerfile` をビルド
3. `https://<service>.onrender.com/html/index.html` で確認。

無料枠は 15 分無アクセスでスピンダウン・起動約1分・**ディスク揮発**（永続化は TiDB へ）。

## TiDB Cloud（予定）

- Serverless（MySQL 互換・**TLS 必須**・port 4000）。接続情報は **env で注入し、リポジトリに秘密を置かない**（`secrets/` は gitignore 済み）。
- Python: `PyMySQL`（TLS 引数が要る）。C++: `USE_MYSQL` ビルド＋`libmysqlclient`（`cpp/Dockerfile` 末尾コメント）。

## ライセンス / 出典

元リポジトリ（passkey_server_cpp）に準じる。cpp-httplib(MIT), nlohmann/json(MIT), SQLite(public domain),
FastAPI/Uvicorn/SQLAlchemy(MIT 等), mbedtls 由来コード(Apache-2.0)。
