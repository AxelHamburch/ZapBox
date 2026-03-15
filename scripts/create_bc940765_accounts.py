#!/usr/bin/env python3
"""
Batch creation of 21 LNbits accounts on 21mio.space
=====================================================
Accounts  : bc940765_01 -- bc940765_21
Password  : bc940765
Wallet    : bc940765-01/21 -- bc940765-21/21
Extension : boltcards

Requirements:
    pip install requests

Usage:
    python3 create_bc940765_accounts.py --user SUPERUSER --password SUPERPASS
    python3 create_bc940765_accounts.py --user SUPERUSER --password SUPERPASS --dry-run
"""

import argparse
import csv
import json
import os
import sys
from datetime import datetime

try:
    import requests
except ImportError:
    sys.exit("Missing dependency. Run:  pip install requests")

# -- Configuration ---------------------------------------------------------------

BASE_URL   = "https://21mio.space"
PREFIX     = "bc940765"
PASSWORD   = "bc940765"
COUNT      = 21
EXTENSION  = "boltcards"

# -- API helpers ------------------------------------------------------------------


def admin_login(username, password):
    """Login as superuser via POST /api/v1/auth and return JWT access token."""
    resp = requests.post(
        f"{BASE_URL}/api/v1/auth",
        json={"username": username, "password": password},
        headers={"Content-Type": "application/json"},
        timeout=15,
    )
    if not resp.ok:
        print(f"\n  DEBUG login {resp.status_code}: {resp.text[:300]}")
    resp.raise_for_status()
    token = resp.json().get("access_token")
    if not token:
        sys.exit("ERROR: Login succeeded but no access_token in response.")
    return token


def create_user(session, username, password):
    """Create a new LNbits user via admin API (POST /users/api/v1/user)."""
    resp = session.post(
        f"{BASE_URL}/users/api/v1/user",
        json={
            "username":        username,
            "password":        password,
            "password_repeat": password,
            "extensions":      [EXTENSION],
        },
        timeout=15,
    )
    if not resp.ok:
        print(f"\n  DEBUG create_user {resp.status_code}: {resp.text[:300]}")
    resp.raise_for_status()
    return resp.json()


def get_user_wallets(session, user_id):
    """Fetch wallets for a user via admin API."""
    resp = session.get(
        f"{BASE_URL}/users/api/v1/user/{user_id}/wallet",
        timeout=15,
    )
    if not resp.ok:
        print(f"\n  DEBUG get_wallets {resp.status_code}: {resp.text[:300]}")
    resp.raise_for_status()
    return resp.json()


def rename_wallet(wallet_adminkey, new_name):
    """Rename a wallet using PATCH /api/v1/wallet with the wallet's own admin key."""
    resp = requests.patch(
        f"{BASE_URL}/api/v1/wallet",
        json={"name": new_name},
        headers={
            "X-Api-Key":    wallet_adminkey,
            "Content-Type": "application/json",
        },
        timeout=10,
    )
    if not resp.ok:
        print(f"\n  DEBUG rename {resp.status_code}: {resp.text[:200]}")
    resp.raise_for_status()
    return resp.json()


# -- Main -------------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(description="Batch-create LNbits accounts")
    parser.add_argument("--user",     help="Superuser username for login")
    parser.add_argument("--password", help="Superuser password for login")
    parser.add_argument("--dry-run",  action="store_true",
                        help="Show what would be created without calling the API")
    args = parser.parse_args()

    su_user = args.user or os.environ.get("LNBITS_ADMIN_USER", "")
    su_pass = args.password or os.environ.get("LNBITS_ADMIN_PASS", "")
    if (not su_user or not su_pass) and not args.dry_run:
        sys.exit(
            "ERROR: Superuser credentials required.\n"
            "  Pass via --user NAME --password PASS\n"
            "  or set LNBITS_ADMIN_USER / LNBITS_ADMIN_PASS"
        )

    # Login as superuser to get JWT access token
    access_token = None
    if not args.dry_run:
        print(f"  Logging in as '{su_user}' ...", end=" ", flush=True)
        access_token = admin_login(su_user, su_pass)
        print("ok\n")

    session = requests.Session()
    session.headers.update({
        "Authorization": f"Bearer {access_token}" if access_token else "",
        "Content-Type":  "application/json",
    })

    results   = []
    errors    = []
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    print(f"\n{'=' * 72}")
    print(f"  LNbits Batch Account Creator  -  {BASE_URL}")
    print(f"  Prefix: {PREFIX}  |  Password: {PASSWORD}  |  Extension: {EXTENSION}")
    print(f"  Count: {COUNT}  |  Dry-run: {'YES' if args.dry_run else 'NO'}")
    print(f"{'=' * 72}\n")

    for i in range(1, COUNT + 1):
        username    = f"{PREFIX}_{i:02d}"
        wallet_name = f"{PREFIX}-{i:02d}/{COUNT}"

        if args.dry_run:
            print(f"  [DRY] Would create: {username}  /  wallet: {wallet_name}")
            results.append({
                "nr": i, "username": username, "password": PASSWORD,
                "wallet_name": wallet_name, "user_id": "DRY-RUN",
                "wallet_id": "DRY-RUN", "inkey": "DRY-RUN",
                "adminkey": "DRY-RUN", "extension": EXTENSION, "status": "dry-run",
            })
            continue

        print(f"  [{i:02d}/{COUNT}] Creating: {username} ... ", end="", flush=True)
        try:
            # 1. Create user (extensions enabled automatically)
            user_data = create_user(session, username, PASSWORD)
            user_id   = user_data["id"]

            # 2. Fetch wallets for the new user
            wallets = get_user_wallets(session, user_id)
            wallet  = wallets[0]

            # 3. Rename default wallet
            rename_wallet(wallet["adminkey"], wallet_name)

            results.append({
                "nr":          i,
                "username":    username,
                "password":    PASSWORD,
                "wallet_name": wallet_name,
                "user_id":     user_id,
                "wallet_id":   wallet["id"],
                "inkey":       wallet["inkey"],
                "adminkey":    wallet["adminkey"],
                "extension":   EXTENSION,
                "status":      "ok",
            })
            print(f"ok  (user_id: {user_id})")

        except requests.HTTPError as exc:
            msg = f"HTTP {exc.response.status_code}: {exc.response.text[:120]}"
            print(f"FAIL  {msg}")
            errors.append({"username": username, "error": msg})
        except Exception as exc:
            print(f"FAIL  {exc}")
            errors.append({"username": username, "error": str(exc)})

    # -- Console summary -----------------------------------------------------------

    if results:
        col = "{:>3} | {:<20} | {:<10} | {:<22} | {:<36} | {}"
        header = col.format("Nr", "Username", "Password", "Wallet name",
                            "User ID", "Status")
        divider = "-" * len(header)
        print(f"\n{divider}")
        print(header)
        print(divider)
        for r in results:
            print(col.format(r["nr"], r["username"], r["password"],
                             r["wallet_name"], r["user_id"], r["status"]))
        print(divider)

    if errors:
        print(f"\n  {len(errors)} error(s):")
        for e in errors:
            print(f"   {e['username']}: {e['error']}")

    # -- CSV output ----------------------------------------------------------------

    if results and not args.dry_run:
        csv_file = f"accounts_{PREFIX}_{timestamp}.csv"
        fieldnames = ["nr", "username", "password", "wallet_name",
                      "user_id", "wallet_id", "inkey", "adminkey",
                      "extension", "status"]
        with open(csv_file, "w", newline="", encoding="utf-8") as fh:
            writer = csv.DictWriter(fh, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(results)
        print(f"\n  Results saved to: {csv_file}")

    # -- JSON output ---------------------------------------------------------------

    if results and not args.dry_run:
        json_file = f"accounts_{PREFIX}_{timestamp}.json"
        with open(json_file, "w", encoding="utf-8") as fh:
            json.dump(results, fh, indent=2, ensure_ascii=False)
        print(f"  Full detail saved to: {json_file}")

    print(f"\n  Done: {len(results)} created, {len(errors)} failed.\n")


if __name__ == "__main__":
    main()
