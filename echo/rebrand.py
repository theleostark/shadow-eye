#!/usr/bin/env python3
"""ECHO Rebrand Engine — transforms TRMNL firmware strings into ECHO identity.

Scans src/, include/, lib/ for TRMNL-branded strings and replaces them
with ECHO equivalents. Runs as a pre-build step in the CI/CD pipeline.

Usage:
    python3 echo/rebrand.py [--dry-run] [--report] [--apply]

Modes:
    --dry-run   Show what would change without modifying files (default)
    --report    Generate rebrand-report.json with all findings
    --apply     Apply all replacements in-place
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field, asdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
VERSION_FILE = Path(__file__).resolve().parent / "version.json"

# ── Rebrand Rules ────────────────────────────────────────────────────
# Each rule: (pattern, replacement, context, severity)
# severity: "critical" = user-visible, "cosmetic" = internal only, "skip" = don't touch

@dataclass
class Rule:
    pattern: str
    replacement: str
    context: str
    severity: str = "critical"  # critical | cosmetic | skip
    regex: bool = False


RULES: list[Rule] = [
    # ── User-visible strings (critical — these appear on the e-paper display) ──
    Rule(
        '"TRMNL firmware ',
        '"ECHO firmware ',
        "WiFi error screen firmware label",
    ),
    Rule(
        '"TRMNL Firmware ',
        '"ECHO Firmware ',
        "Case variant of firmware label",
    ),
    Rule(
        "TRMNL firmware 2.1.0",
        "ECHO 0.0.1-alpha",
        "Hardcoded version string on error display",
    ),
    Rule(
        '"Can\'t establish WiFi connection."',
        '"Cannot connect to WiFi."',
        "WiFi error message (cleaner English)",
    ),
    Rule(
        '"Hold button on the back to reset WiFi, or scan QR Code for help."',
        '"Hold side button 10s to reset WiFi. Visit shadowlab.cc/echo for help."',
        "WiFi help text — point to our docs, not TRMNL",
    ),

    # ── Captive portal HTML (critical — visible in browser) ──
    Rule(
        "<title>TRMNL</title>",
        "<title>ECHO Setup</title>",
        "Captive portal page title",
    ),
    Rule(
        r'src=".*trmnl-logo.*?"',
        'src="/echo-logo.svg"',
        "Captive portal logo image reference",
        regex=True,
    ),
    Rule(
        "TRMNL</h1>",
        "ECHO</h1>",
        "Captive portal heading",
    ),
    Rule(
        "usetrmnl.com",
        "shadowlab.cc/echo",
        "Help URL references",
        severity="critical",
    ),

    # ── Log messages (cosmetic — only visible in serial monitor) ──
    Rule(
        '"[TRMNL]',
        '"[ECHO]',
        "Serial log prefix",
        severity="cosmetic",
    ),
    Rule(
        "TRMNL device",
        "ECHO device",
        "Log messages referring to device name",
        severity="cosmetic",
    ),

    # ── mDNS / network identity ──
    Rule(
        '"trmnl"',
        '"echo"',
        "mDNS service name (only in mdns_discovery.cpp)",
        severity="critical",
    ),

    # ── Strings we MUST NOT touch ──
    # These reference the TRMNL API/infrastructure we still depend on
    # Rule("trmnl.com/api",   SKIP — we use their API)
    # Rule("trmnl.app/api",   SKIP — device endpoint)
    # Rule("TRMNL" in enum names, SKIP — internal code identifiers)
]

# Files/patterns to NEVER modify
SKIP_PATHS = {
    ".git",
    ".pio",
    "echo/rebrand.py",  # don't rebrand ourselves
    "node_modules",
}

SKIP_EXTENSIONS = {
    ".bin", ".elf", ".o", ".d", ".map", ".a",
    ".png", ".jpg", ".bmp", ".svg",
    ".json",  # config files handled separately
}

# Only touch these extensions
SCAN_EXTENSIONS = {".cpp", ".h", ".c", ".ino", ".html", ".htm", ".css", ".js"}


@dataclass
class Finding:
    file: str
    line: int
    rule_pattern: str
    rule_replacement: str
    context_text: str
    severity: str


@dataclass
class Report:
    total_files_scanned: int = 0
    total_findings: int = 0
    critical: int = 0
    cosmetic: int = 0
    findings: list[Finding] = field(default_factory=list)


def should_skip(path: Path) -> bool:
    parts = path.relative_to(REPO_ROOT).parts
    return any(skip in parts for skip in SKIP_PATHS) or path.suffix in SKIP_EXTENSIONS


def scan_file(path: Path, rules: list[Rule]) -> list[Finding]:
    findings = []
    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return findings

    lines = content.split("\n")
    rel = str(path.relative_to(REPO_ROOT))

    for i, line in enumerate(lines, 1):
        # Skip lines that reference TRMNL API endpoints (we still use them)
        if "trmnl.com/api" in line or "trmnl.app/api" in line:
            continue
        # Skip TRMNL in #include or import paths
        if line.strip().startswith("#include") or line.strip().startswith("//"):
            continue

        for rule in rules:
            if rule.severity == "skip":
                continue
            if rule.regex:
                if re.search(rule.pattern, line):
                    findings.append(Finding(
                        file=rel, line=i,
                        rule_pattern=rule.pattern,
                        rule_replacement=rule.replacement,
                        context_text=line.strip()[:120],
                        severity=rule.severity,
                    ))
            else:
                if rule.pattern in line:
                    findings.append(Finding(
                        file=rel, line=i,
                        rule_pattern=rule.pattern,
                        rule_replacement=rule.replacement,
                        context_text=line.strip()[:120],
                        severity=rule.severity,
                    ))
    return findings


def apply_rules(path: Path, rules: list[Rule]) -> int:
    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return 0

    original = content
    changes = 0

    for rule in rules:
        if rule.severity == "skip":
            continue

        # Protect TRMNL API references
        lines = content.split("\n")
        new_lines = []
        for line in lines:
            if "trmnl.com/api" in line or "trmnl.app/api" in line:
                new_lines.append(line)
                continue
            if line.strip().startswith("#include"):
                new_lines.append(line)
                continue

            if rule.regex:
                new_line = re.sub(rule.pattern, rule.replacement, line)
            else:
                new_line = line.replace(rule.pattern, rule.replacement)

            if new_line != line:
                changes += 1
            new_lines.append(new_line)

        content = "\n".join(new_lines)

    if content != original:
        path.write_text(content, encoding="utf-8")

    return changes


def main():
    parser = argparse.ArgumentParser(description="ECHO Rebrand Engine")
    parser.add_argument("--dry-run", action="store_true", default=True,
                        help="Show findings without modifying files (default)")
    parser.add_argument("--apply", action="store_true",
                        help="Apply replacements in-place")
    parser.add_argument("--report", action="store_true",
                        help="Write rebrand-report.json")
    args = parser.parse_args()

    if args.apply:
        args.dry_run = False

    # Load version for display
    version = json.loads(VERSION_FILE.read_text()) if VERSION_FILE.exists() else {}
    v = version.get("version", {})
    echo_ver = f"{v.get('major', 0)}.{v.get('minor', 0)}.{v.get('patch', 1)}-{v.get('tag', 'alpha')}"

    print(f"ECHO Rebrand Engine — targeting ECHO {echo_ver}")
    print(f"Base: TRMNL {version.get('base', {}).get('version', '?')}")
    print(f"Rules: {len(RULES)} ({sum(1 for r in RULES if r.severity == 'critical')} critical)")
    print()

    # Scan
    report = Report()
    scan_dirs = [REPO_ROOT / d for d in ("src", "include", "lib")]
    files = []
    for d in scan_dirs:
        if d.exists():
            files.extend(f for f in d.rglob("*") if f.is_file() and f.suffix in SCAN_EXTENSIONS)

    for f in sorted(files):
        if should_skip(f):
            continue
        report.total_files_scanned += 1

        if args.dry_run:
            findings = scan_file(f, RULES)
            report.findings.extend(findings)
        else:
            changes = apply_rules(f, RULES)
            if changes:
                rel = str(f.relative_to(REPO_ROOT))
                print(f"  PATCHED {rel} ({changes} replacements)")
                report.total_findings += changes

    if args.dry_run:
        report.total_findings = len(report.findings)
        report.critical = sum(1 for f in report.findings if f.severity == "critical")
        report.cosmetic = sum(1 for f in report.findings if f.severity == "cosmetic")

        print(f"Scanned: {report.total_files_scanned} files")
        print(f"Findings: {report.total_findings} ({report.critical} critical, {report.cosmetic} cosmetic)")
        print()

        for f in report.findings:
            sev = "!!" if f.severity == "critical" else ".."
            print(f"  {sev} {f.file}:{f.line}")
            print(f"     {f.context_text}")
            print(f"     {f.rule_pattern} → {f.rule_replacement}")
            print()

    if args.report:
        out = REPO_ROOT / "echo" / "rebrand-report.json"
        out.write_text(json.dumps({
            "echo_version": echo_ver,
            "scanned": report.total_files_scanned,
            "findings": report.total_findings,
            "critical": report.critical,
            "cosmetic": report.cosmetic,
            "details": [asdict(f) for f in report.findings],
        }, indent=2))
        print(f"Report written to {out.relative_to(REPO_ROOT)}")

    if not args.dry_run:
        print(f"\nApplied {report.total_findings} replacements across {report.total_files_scanned} files")

    return 0 if report.critical == 0 or not args.dry_run else 1


if __name__ == "__main__":
    sys.exit(main())
