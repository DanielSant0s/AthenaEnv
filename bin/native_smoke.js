// {"name": "Native compiler smoke test", "author": "Daniel Santos", "file": "native_smoke.js", "category": "system", "blurb": "Regression suite for the AOT native JS compiler.", "exit": "TRIANGLE"}
//
// Regression suite for the AOT native compiler (src/native_compiler) and the
// float32 extensions in the QuickJS fork. Every case runs the same computation
// twice - once through Native.compile, once in plain JS - and diffs the
// results, so a failure names the exact feature rather than just "the game
// looks wrong".
//
// Run it after touching anything in src/native_compiler/, src/quickjs/, or
// src/js_api/ath_native.c. Expected output: "0 failed".
//
// Cases tagged [reg] pin a bug that actually shipped and was fixed; don't
// delete those without understanding what they caught.
//
// Deliberately NOT covered: a JS-vs-native timing benchmark. It has no
// pass/fail, and wall-clock numbers make a regression suite flaky.

import * as Native from 'athena_native.erl';
import { Terminal, exitToShell } from 'athenaos/app.js';

let passed = 0;
const failures = [];

function ok(name, cond, detail) {
    if (cond) { passed++; return true; }
    failures.push(`${name}${detail ? "  (" + detail + ")" : ""}`);
    return false;
}

function eq(name, got, want, tol) {
    const t = (tol === undefined) ? 0.0005 : tol;
    return ok(name, Math.abs(got - want) <= t, `got=${got} want=${want}`);
}

function eqi(name, got, want) {
    return ok(name, got === want, `got=${got} want=${want}`);
}

function eqs(name, got, want) {
    return ok(name, got === want, `got="${got}" want="${want}"`);
}

function eqArr(name, got, want, tol) {
    const t = (tol === undefined) ? 0.0005 : tol;
    for (let i = 0; i < want.length; i++) {
        if (Math.abs(got[i] - want[i]) > t) {
            return ok(name, false, `[${i}] got=${got[i]} want=${want[i]}`);
        }
    }
    return ok(name, true);
}

// A compile failure is reported as a failed case instead of aborting the run -
// one broken feature shouldn't hide the state of everything else.
function compile(name, sig, fn) {
    try {
        return Native.compile(sig, fn);
    } catch (e) {
        failures.push(`${name}  (failed to compile: ${e})`);
        return null;
    }
}

// Same idea for anything else that can throw (Native.struct, dynamic arrays).
function attempt(name, fn) {
    try {
        return fn();
    } catch (e) {
        failures.push(`${name}  (threw: ${e})`);
        return null;
    }
}

const F1 = ["float"], F2 = ["float", "float"], F3 = ["float", "float", "float"];
const I2 = ["int", "int"];

// ---------------------------------------------------------------------------
// 1. Scalar arithmetic
// ---------------------------------------------------------------------------
{
    const addI = compile("int add", { args: I2, returns: "int" },
        function (a, b) { return a + b; });
    if (addI) { eqi("1.1 int add", addI(17, 25), 42); eqi("1.2 int add negative", addI(-9, 4), -5); }

    const arith = compile("int arith", { args: I2, returns: "int" },
        function (a, b) { return (a * b - a) / 2; });
    if (arith) eqi("1.3 int mul/sub/div", arith(10, 7), 30);

    const modI = compile("int mod", { args: I2, returns: "int" },
        function (a, b) { return a % b; });
    if (modI) { eqi("1.4 int mod", modI(17, 5), 2); eqi("1.5 int mod negative", modI(-17, 5), -2); }

    const addF = compile("float add", { args: F2, returns: "float" },
        function (a, b) { return a + b; });
    if (addF) eq("1.6 float add", addF(0.25f, 0.5f), 0.75);

    const chain = compile("float chain", { args: F3, returns: "float" },
        function (a, b, c) { return (a + b) * c - a / c; });
    if (chain) eq("1.7 float chain", chain(1.5f, 2.5f, 4.0f), (1.5 + 2.5) * 4.0 - 1.5 / 4.0);

    const bits = compile("bitwise", { args: I2, returns: "int" },
        function (a, b) { return ((a & b) | (a ^ b)) + (a << 2) + (a >> 1); });
    if (bits) eqi("1.8 bitwise", bits(12, 10), ((12 & 10) | (12 ^ 10)) + (12 << 2) + (12 >> 1));

    const neg = compile("negate", { args: F1, returns: "float" },
        function (a) { return -a; });
    if (neg) eq("1.9 [reg] float negate", neg(3.25f), -3.25);

    const negI = compile("int negate", { args: ["int"], returns: "int" },
        function (a) { return -a; });
    if (negI) eqi("1.10 int negate", negI(42), -42);

    // Several live locals across a loop, plus an early return.
    const fib = compile("fibonacci", { args: ["int"], returns: "int" },
        function (n) {
            if (n <= 1) return n;
            let a = 0;
            let b = 1;
            for (let i = 2; i <= n; i++) {
                const t = a + b;
                a = b;
                b = t;
            }
            return b;
        });
    if (fib) { eqi("1.11 fibonacci(10)", fib(10), 55); eqi("1.12 fibonacci(20)", fib(20), 6765); }
}

// ---------------------------------------------------------------------------
// 2. Whole-number float literals  [reg]
// QuickJS emits the same compact integer-push opcode for `6.0f` as for `6`, so
// the `f` suffix never reaches the compiler. Anything treating such a value as
// float bits without a numeric conversion reads garbage (a NaN, or an integer
// divide where a float divide was meant).
// ---------------------------------------------------------------------------
{
    const retWhole = compile("ret whole float", { args: F1, returns: "float" },
        function (a) { return 2.0f; });
    if (retWhole) eq("2.1 [reg] return whole float literal", retWhole(0.0f), 2.0);

    const retNegWhole = compile("ret neg whole", { args: F1, returns: "float" },
        function (a) { return -1.0f; });
    if (retNegWhole) eq("2.2 [reg] return -1.0f (used to be NaN)", retNegWhole(0.0f), -1.0);

    const mulWhole = compile("mul whole", { args: F1, returns: "float" },
        function (a) { return a * 3.0f; });
    if (mulWhole) eq("2.3 [reg] multiply by whole literal", mulWhole(1.5f), 4.5);

    const divWhole = compile("div whole", { args: F1, returns: "float" },
        function (a) { return a / 4.0f; });
    if (divWhole) eq("2.4 [reg] divide by whole literal", divWhole(3.0f), 0.75);

    // An int loop counter over a float divisor must be a float divide, not a
    // truncating integer one.
    const intOverFloat = compile("int/float", { args: F1, returns: "float" },
        function (d) {
            const i = 3;
            return i / d;
        });
    if (intOverFloat) eq("2.5 [reg] int / float does not truncate", intOverFloat(2.0f), 1.5);

    const cmpWhole = compile("cmp whole", { args: F1, returns: "bool" },
        function (a) { return a < 5.0f; });
    if (cmpWhole) {
        ok("2.6 [reg] compare against whole literal (true)", cmpWhole(1.0f) === true);
        ok("2.7 [reg] compare against whole literal (false)", cmpWhole(9.0f) === false);
    }
}

// ---------------------------------------------------------------------------
// 3. Comparisons and bool
// ---------------------------------------------------------------------------
{
    const ltF = compile("lt", { args: F2, returns: "bool" }, function (a, b) { return a < b; });
    if (ltF) {
        ok("3.1 lt true", ltF(0.0f, 1.0f) === true);
        ok("3.2 lt false", ltF(1.0f, 0.0f) === false);
        ok("3.3 lt equal", ltF(2.5f, 2.5f) === false);
        // Negative floats: broken back when float compares used integer SLT.
        ok("3.4 -2 < -1", ltF(-2.0f, -1.0f) === true);
        ok("3.5 -1 < -2 is false", ltF(-1.0f, -2.0f) === false);
    }

    const rel = compile("rel ops", { args: F2, returns: "int" },
        function (a, b) {
            let n = 0;
            if (a < b) n = n + 1;
            if (a <= b) n = n + 2;
            if (a > b) n = n + 4;
            if (a >= b) n = n + 8;
            if (a === b) n = n + 16;
            return n;
        });
    if (rel) {
        eqi("3.6 relational a<b", rel(1.0f, 2.0f), 1 + 2);
        eqi("3.7 relational a>b", rel(2.0f, 1.0f), 4 + 8);
        eqi("3.8 relational a==b", rel(2.0f, 2.0f), 2 + 8 + 16);
    }

    const cmpI = compile("int cmp", { args: I2, returns: "bool" },
        function (a, b) { return a >= b; });
    if (cmpI) { ok("3.9 int >=", cmpI(5, 5) === true); ok("3.10 int >= false", cmpI(4, 5) === false); }

    const isEven = compile("isEven", { args: ["int"], returns: "bool" },
        function (n) { return (n % 2) === 0; });
    if (isEven) { ok("3.11 bool return true", isEven(4) === true); ok("3.12 bool return false", isEven(7) === false); }

    // Unsigned: values above 0x7FFFFFFF must not compare as negative.
    const uLt = compile("uint lt", { args: ["uint", "uint"], returns: "bool" },
        function (a, b) { return a < b; });
    const uGt = compile("uint gt", { args: ["uint", "uint"], returns: "bool" },
        function (a, b) { return a > b; });
    if (uLt) {
        ok("3.13 uint 0x80000001 < 0x80000002", uLt(0x80000001, 0x80000002) === true);
        ok("3.14 uint 0x80000000 < 0x80000001", uLt(0x80000000, 0x80000001) === true);
    }
    if (uGt) ok("3.15 uint 0x80000001 > 0x80000000", uGt(0x80000001, 0x80000000) === true);
}

// ---------------------------------------------------------------------------
// 4. Control flow
// ---------------------------------------------------------------------------
{
    const branch = compile("if/else", { args: F1, returns: "float" },
        function (a) { if (a > 0.0f) return 1.5f; else return -1.5f; });
    if (branch) { eq("4.1 if", branch(1.0f), 1.5); eq("4.2 [reg] else with negated literal", branch(-1.0f), -1.5); }

    // Ternary: both arms feed one merge point. [reg] the merge used to reset
    // the eval stack to empty and drop the incoming value.
    const tern = compile("ternary", { args: F2, returns: "float" },
        function (a, b) { const v = (a > b) ? a : b; return v * 2.0f; });
    if (tern) { eq("4.3 [reg] ternary A", tern(3.0f, 1.0f), 6.0); eq("4.4 [reg] ternary B", tern(1.0f, 4.0f), 8.0); }

    const loop = compile("loop", { args: ["int"], returns: "int" },
        function (n) { let s = 0; for (let i = 0; i < n; i++) s = s + i; return s; });
    if (loop) { eqi("4.5 loop sum", loop(5), 10); eqi("4.6 loop zero trips", loop(0), 0); }

    // [reg] a loop's back-edge once landed on the entry block, which re-ran the
    // initialiser every iteration and hung the machine.
    const loopBack = compile("loop backedge", { args: ["int"], returns: "int" },
        function (n) { let s = 0; for (let i = 0; i <= n; i++) { s = s + 1; } return s; });
    if (loopBack) eqi("4.7 [reg] back-edge does not reset the counter", loopBack(6), 7);

    const brk = compile("break", { args: ["int"], returns: "int" },
        function (n) { let s = 0; for (let i = 0; i < n; i++) { if (i === 3) break; s = s + i; } return s; });
    if (brk) eqi("4.8 break", brk(10), 0 + 1 + 2);

    const cont = compile("continue", { args: ["int"], returns: "int" },
        function (n) { let s = 0; for (let i = 0; i < n; i++) { if (i === 2) continue; s = s + i; } return s; });
    if (cont) eqi("4.9 continue", cont(5), 0 + 1 + 3 + 4);

    // [reg] `continue` first, then a large if/else-if/else body: the fallthrough
    // successor was clobbered and the whole body became unreachable dead code.
    const contBig = compile("continue+body", { args: I2, returns: "int" },
        function (n, k) {
            let s = 0;
            for (let i = 0; i < n; i++) {
                if (i === 0) continue;
                if (k === 0) { s = s + 1; }
                else if (k === 1) { s = s + 10; }
                else if (k === 2) { s = s + 100; }
                else { s = s + 1000; }
            }
            return s;
        });
    if (contBig) {
        eqi("4.10 [reg] continue + large body k=0", contBig(4, 0), 3);
        eqi("4.11 [reg] continue + large body k=2", contBig(4, 2), 300);
        eqi("4.12 [reg] continue + large body k=9", contBig(4, 9), 3000);
    }

    const nested = compile("nested loop", { args: ["int"], returns: "int" },
        function (n) {
            let s = 0;
            for (let i = 0; i < n; i++) for (let j = 0; j < n; j++) s = s + 1;
            return s;
        });
    if (nested) eqi("4.13 nested loop", nested(4), 16);

    const chainIf = compile("if chain", { args: ["int"], returns: "int" },
        function (k) {
            if (k === 0) return 10;
            else if (k === 1) return 20;
            else if (k === 2) return 30;
            else if (k === 3) return 40;
            return 99;
        });
    if (chainIf) {
        eqi("4.14 if chain 0", chainIf(0), 10);
        eqi("4.15 if chain 2", chainIf(2), 30);
        eqi("4.16 if chain default", chainIf(7), 99);
    }
}

// ---------------------------------------------------------------------------
// 5. Optimizer passes
// ---------------------------------------------------------------------------
{
    // Constant folding: no arguments at all, result known at compile time.
    const constExpr = compile("const fold", { args: [], returns: "int" },
        function () { return 10 + 5 * 2; });
    if (constExpr) eqi("5.1 constant folding (zero-arg function)", constExpr(), 20);

    // Strength reduction: these become shifts, but must still be correct.
    const mulBy8 = compile("mul by 8", { args: ["int"], returns: "int" },
        function (a) { return a * 8; });
    if (mulBy8) { eqi("5.2 strength reduction a*8", mulBy8(5), 40); eqi("5.3 a*8 negative", mulBy8(-3), -24); }

    const divBy4 = compile("div by 4", { args: ["int"], returns: "int" },
        function (a) { return (a / 4) | 0; });
    if (divBy4) eqi("5.4 strength reduction a/4", divBy4(100), 25);
}

// ---------------------------------------------------------------------------
// 6. Math intrinsics (inlined, no C call)
// ---------------------------------------------------------------------------
{
    const sq = compile("sqrt", { args: F1, returns: "float" }, function (a) { return Math.sqrt(a); });
    if (sq) { eq("6.1 sqrt(9)", sq(9.0f), 3.0); eq("6.2 sqrt(2)", sq(2.0f), Math.sqrt(2), 0.001); }

    const ab = compile("abs", { args: F1, returns: "float" }, function (a) { return Math.abs(a); });
    if (ab) { eq("6.3 abs negative", ab(-2.5f), 2.5); eq("6.4 abs positive", ab(2.5f), 2.5); }

    const mn = compile("min", { args: F2, returns: "float" }, function (a, b) { return Math.min(a, b); });
    if (mn) { eq("6.5 min", mn(3.0f, 1.5f), 1.5); eq("6.6 min negative", mn(-2.0f, 7.0f), -2.0); }

    const mx = compile("max", { args: F2, returns: "float" }, function (a, b) { return Math.max(a, b); });
    if (mx) { eq("6.7 max", mx(3.0f, 1.5f), 3.0); eq("6.8 max both negative", mx(-10.5f, -3.2f), -3.2); }

    // Variadic min/max expand to a chain of binary ops; 6 args leaves less room
    // on the 8-slot eval stack for the intermediates.
    const mn4 = compile("min4", { args: ["float", "float", "float", "float"], returns: "float" },
        function (a, b, c, d) { return Math.min(a, b, c, d); });
    if (mn4) eq("6.9 variadic min (4 args)", mn4(5.0f, 3.0f, 8.0f, 1.0f), 1.0);

    const mx4 = compile("max4", { args: ["float", "float", "float", "float"], returns: "float" },
        function (a, b, c, d) { return Math.max(a, b, c, d); });
    if (mx4) eq("6.10 variadic max (4 args)", mx4(5.0f, 3.0f, 8.0f, 1.0f), 8.0);

    const mn6 = compile("min6", { args: ["float", "float", "float", "float", "float", "float"], returns: "float" },
        function (a, b, c, d, e, f) { return Math.min(a, b, c, d, e, f); });
    if (mn6) eq("6.11 variadic min (6 args)", mn6(10.0f, 5.0f, 20.0f, 3.0f, 15.0f, 8.0f), 3.0);

    const mx6 = compile("max6", { args: ["float", "float", "float", "float", "float", "float"], returns: "float" },
        function (a, b, c, d, e, f) { return Math.max(a, b, c, d, e, f); });
    if (mx6) eq("6.12 variadic max (6 args)", mx6(10.0f, 5.0f, 20.0f, 3.0f, 15.0f, 8.0f), 20.0);

    const cl = compile("clamp", { args: F3, returns: "float" }, function (v, lo, hi) { return Math.clamp(v, lo, hi); });
    if (cl) {
        eq("6.13 clamp inside", cl(0.5f, 0.0f, 1.0f), 0.5);
        eq("6.14 clamp above", cl(5.0f, 0.0f, 1.0f), 1.0);
        eq("6.15 clamp below", cl(-5.0f, 0.0f, 1.0f), 0.0);
        eq("6.16 clamp all equal", cl(0.0f, 0.0f, 0.0f), 0.0);
    }

    const lp = compile("lerp", { args: F3, returns: "float" }, function (a, b, t) { return Math.lerp(a, b, t); });
    if (lp) {
        eq("6.17 lerp t=0", lp(2.0f, 10.0f, 0.0f), 2.0);
        eq("6.18 lerp t=0.5", lp(0.0f, 10.0f, 0.5f), 5.0);
        eq("6.19 lerp across zero", lp(-5.0f, 5.0f, 0.5f), 0.0);
    }

    const sg = compile("sign", { args: F1, returns: "float" }, function (a) { return Math.sign(a); });
    if (sg) {
        eq("6.20 sign positive", sg(3.0f), 1.0);
        eq("6.21 sign negative", sg(-3.0f), -1.0);
        eq("6.22 sign zero", sg(0.0f), 0.0);
    }

    const fr = compile("fround", { args: F1, returns: "float" }, function (a) { return Math.fround(a); });
    if (fr) { eq("6.23 fround(1.5)", fr(1.5f), 1.5); eq("6.24 fround(1.337)", fr(1.337f), 1.337, 0.001); }

    const sat = compile("saturate", { args: F1, returns: "float" }, function (a) { return Math.saturate(a); });
    if (sat) {
        eq("6.25 saturate high", sat(2.0f), 1.0);
        eq("6.26 saturate low", sat(-2.0f), 0.0);
        eq("6.27 saturate inside", sat(0.5f), 0.5);
    }

    const st = compile("step", { args: F2, returns: "float" }, function (e, x) { return Math.step(e, x); });
    if (st) {
        eq("6.28 step above", st(0.5f, 0.7f), 1.0);
        eq("6.29 step equal", st(0.5f, 0.5f), 1.0);
        eq("6.30 step below", st(0.5f, 0.3f), 0.0);
    }

    const ss = compile("smoothstep", { args: F3, returns: "float" }, function (a, b, x) { return Math.smoothstep(a, b, x); });
    if (ss) {
        eq("6.31 smoothstep low", ss(0.0f, 1.0f, 0.0f), 0.0);
        eq("6.32 smoothstep mid", ss(0.0f, 1.0f, 0.5f), 0.5);
        eq("6.33 smoothstep high", ss(0.0f, 1.0f, 1.0f), 1.0);
        eq("6.34 smoothstep scaled", ss(0.0f, 10.0f, 5.0f), 0.5);
    }

    const rs = compile("rsqrt", { args: F1, returns: "float" }, function (a) { return Math.rsqrt(a); });
    if (rs) { eq("6.35 rsqrt(4)", rs(4.0f), 0.5, 0.01); eq("6.36 rsqrt(0.25)", rs(0.25f), 2.0, 0.01); }

    const im = compile("imul", { args: I2, returns: "int" }, function (a, b) { return Math.imul(a, b); });
    if (im) {
        eqi("6.37 [reg] imul (tail call)", im(7, 6), 42);
        eqi("6.38 imul negative", im(-5, 3), -15);
        eqi("6.39 imul wraps to 32 bits", im(0xFFFFFFFF, 5), -5);
    }

    const im2 = compile("imul local", { args: I2, returns: "int" },
        function (a, b) { const r = Math.imul(a, b); return r; });
    if (im2) eqi("6.40 [reg] imul stored in a local", im2(7, 6), 42);

    const im3 = compile("imul arith", { args: I2, returns: "int" },
        function (a, b) { return Math.imul(a, b) + 1; });
    if (im3) eqi("6.41 [reg] imul feeding arithmetic", im3(7, 6), 43);

    const fm = compile("fma", { args: F3, returns: "float" }, function (a, b, c) { return Math.fma(a, b, c); });
    if (fm) {
        eq("6.42 fma", fm(2.0f, 3.0f, 1.0f), 7.0);
        eq("6.43 fma fractional", fm(1.5f, 2.0f, 0.5f), 3.5);
        eq("6.44 fma negative", fm(-1.0f, 5.0f, 3.0f), -2.0);
    }
}

// ---------------------------------------------------------------------------
// 7. Registered C functions (IR_CALL_C_FUNC)  [reg]
// This whole family went untested for a long time: the op had no case in type
// inference, so sinf()'s float return came back typed as int and every float op
// consuming it was emitted as integer arithmetic.
// ---------------------------------------------------------------------------
{
    const sn = compile("sin", { args: F1, returns: "float" }, function (a) { return Math.sin(a); });
    if (sn) { eq("7.1 [reg] sin(0)", sn(0.0f), 0.0); eq("7.2 [reg] sin(pi/2)", sn(1.5707964f), 1.0, 0.001); }

    const cs = compile("cos", { args: F1, returns: "float" }, function (a) { return Math.cos(a); });
    if (cs) { eq("7.3 cos(0)", cs(0.0f), 1.0); eq("7.4 cos(pi)", cs(3.1415927f), -1.0, 0.001); }

    const tn = compile("tan", { args: F1, returns: "float" }, function (a) { return Math.tan(a); });
    if (tn) { eq("7.5 tan(0)", tn(0.0f), 0.0); eq("7.6 tan(pi/4)", tn(0.7853982f), 1.0, 0.001); }

    // [reg] a C call's result feeding further float arithmetic - this is what
    // silently became integer maths and returned 0.
    const sinMath = compile("sin arith", { args: F1, returns: "float" },
        function (a) {
            const s = Math.sin(a);
            return s * 2.4f + 0.35f;
        });
    if (sinMath) eq("7.7 [reg] C result in float arithmetic", sinMath(1.5707964f), 1.0 * 2.4 + 0.35, 0.001);

    const as = compile("asin", { args: F1, returns: "float" }, function (a) { return Math.asin(a); });
    if (as) eq("7.8 asin(0.5)", as(0.5f), Math.asin(0.5), 0.001);

    const ac = compile("acos", { args: F1, returns: "float" }, function (a) { return Math.acos(a); });
    if (ac) eq("7.9 acos(0.5)", ac(0.5f), Math.acos(0.5), 0.001);

    const ex = compile("exp", { args: F1, returns: "float" }, function (a) { return Math.exp(a); });
    if (ex) eq("7.10 exp(1)", ex(1.0f), Math.exp(1), 0.001);

    const lg = compile("log", { args: F1, returns: "float" }, function (a) { return Math.log(a); });
    if (lg) eq("7.11 log(e)", lg(2.7182817f), 1.0, 0.001);

    const fl = compile("floor", { args: F1, returns: "float" }, function (a) { return Math.floor(a); });
    if (fl) { eq("7.12 floor(3.7)", fl(3.7f), 3.0); eq("7.13 floor(-3.2)", fl(-3.2f), -4.0); }

    const ce = compile("ceil", { args: F1, returns: "float" }, function (a) { return Math.ceil(a); });
    if (ce) { eq("7.14 ceil(3.2)", ce(3.2f), 4.0); eq("7.15 ceil(-2.7)", ce(-2.7f), -2.0); }

    const rd = compile("round", { args: F1, returns: "float" }, function (a) { return Math.round(a); });
    if (rd) { eq("7.16 round(3.7)", rd(3.7f), 4.0); eq("7.17 round(3.2)", rd(3.2f), 3.0); }

    const tr = compile("trunc", { args: F1, returns: "float" }, function (a) { return Math.trunc(a); });
    if (tr) { eq("7.18 trunc(3.7)", tr(3.7f), 3.0); eq("7.19 trunc(-3.7)", tr(-3.7f), -3.0); }

    const cb = compile("cbrt", { args: F1, returns: "float" }, function (a) { return Math.cbrt(a); });
    if (cb) { eq("7.20 cbrt(8)", cb(8.0f), 2.0, 0.001); eq("7.21 cbrt(27)", cb(27.0f), 3.0, 0.001); }

    const pw = compile("pow", { args: F2, returns: "float" }, function (a, b) { return Math.pow(a, b); });
    if (pw) { eq("7.22 pow(2,10)", pw(2.0f, 10.0f), 1024.0, 0.01); eq("7.23 pow(10,0.5)", pw(10.0f, 0.5f), Math.sqrt(10), 0.001); }

    const at2 = compile("atan2", { args: F2, returns: "float" }, function (y, x) { return Math.atan2(y, x); });
    if (at2) { eq("7.24 atan2(1,1)", at2(1.0f, 1.0f), Math.atan2(1, 1), 0.001); eq("7.25 atan2(1,0)", at2(1.0f, 0.0f), Math.atan2(1, 0), 0.001); }

    const hy = compile("hypot", { args: F2, returns: "float" }, function (a, b) { return Math.hypot(a, b); });
    if (hy) { eq("7.26 hypot(3,4)", hy(3.0f, 4.0f), 5.0, 0.001); eq("7.27 hypot(5,12)", hy(5.0f, 12.0f), 13.0, 0.001); }

    // [reg] Math.xxx pushes a global-object placeholder that the call never
    // popped. A single leftover sat harmlessly below the arguments, but a
    // nested call left the inner one BETWEEN the outer call's arguments, so
    // atan2 read a 0 as its first argument and returned 0.
    const nestedIntrinsic = compile("atan2(sqrt)", { args: F2, returns: "float" },
        function (a, b) { return Math.atan2(-a, Math.sqrt(b * b)); });
    if (nestedIntrinsic) eq("7.28 [reg] intrinsic nested in a C call", nestedIntrinsic(1.0f, 1.0f), Math.atan2(-1, 1), 0.001);

    const nestedC = compile("sqrt(sin)", { args: F1, returns: "float" },
        function (a) { return Math.sqrt(Math.abs(Math.sin(a))); });
    if (nestedC) eq("7.29 [reg] C call nested in an intrinsic", nestedC(1.5707964f), 1.0, 0.001);

    const nestedTwoC = compile("pow(sin,2)", { args: F1, returns: "float" },
        function (a) { return Math.pow(Math.abs(Math.sin(a)), 2.0f); });
    if (nestedTwoC) eq("7.30 [reg] C call nested in a C call", nestedTwoC(1.5707964f), 1.0, 0.001);
}

// ---------------------------------------------------------------------------
// 8. Calls: native -> native, argument counts, tail calls
// ---------------------------------------------------------------------------
const addTwo = compile("helper add2", { args: F2, returns: "float" },
    function (a, b) { return a + b; });

const isLess = compile("helper isLess", { args: F2, returns: "bool" },
    function (a, b) { return a < b; });

const dist2 = compile("helper dist2", { args: ["float", "float", "float", "float", "float", "float"], returns: "float" },
    function (ax, ay, az, bx, by, bz) {
        const dx = ax - bx, dy = ay - by, dz = az - bz;
        return dx * dx + dy * dy + dz * dz;
    });

const dbl = compile("helper double", { args: ["int"], returns: "int" }, function (x) { return x * 2; });
{
    // Stack-passed arguments: a0-a3 in registers, 5+ on the stack.
    const arg5 = compile("5 args", { args: ["float", "float", "float", "float", "float"], returns: "float" },
        function (a, b, c, d, e) { return e; });
    if (arg5) eq("8.1 [reg] 5th argument (stack-passed)", arg5(1.0f, 2.0f, 3.0f, 4.0f, 99.0f), 99.0);

    const arg7 = compile("7 args", { args: ["float", "float", "float", "float", "float", "float", "float"], returns: "float" },
        function (a, b, c, d, e, f, g) { return g; });
    if (arg7) eq("8.2 [reg] 7th argument", arg7(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 77.0f), 77.0);

    const arg8 = compile("8 args sum", { args: ["float", "float", "float", "float", "float", "float", "float", "float"], returns: "float" },
        function (a, b, c, d, e, f, g, h) { return a + b + c + d + e + f + g + h; });
    if (arg8) eq("8.3 8 arguments (the limit)", arg8(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f), 36.0);

    if (addTwo) {
        const callNested = compile("nested call", { args: F2, returns: "float" },
            function (a, b) { const s = addTwo(a, b); return s * 2.0f; });
        if (callNested) eq("8.4 nested native call", callNested(1.5f, 2.5f), 8.0);

        const tailCall = compile("tail call", { args: F2, returns: "float" },
            function (a, b) { return addTwo(a, b); });
        if (tailCall) eq("8.5 tail call", tailCall(3.0f, 4.0f), 7.0);
    }

    if (dbl) {
        const quad = compile("call in call", { args: ["int"], returns: "int" },
            function (x) { return dbl(dbl(x)); });
        if (quad) { eqi("8.6 native call inside a native call", quad(5), 20); eqi("8.7 same, zero", quad(0), 0); }
    }

    if (dist2) {
        const call6 = compile("call 6 args", { args: F3, returns: "float" },
            function (x, y, z) { return dist2(x, y, z, 0.0f, 0.0f, 0.0f); });
        if (call6) eq("8.8 [reg] nested call with 6 arguments", call6(1.0f, 2.0f, 2.0f), 9.0);
    }

    // [reg] the peephole fused a compare's true-arm with the return move and
    // NOPed the move out - but that move was a branch target, so the FALSE path
    // never wrote $v0 and returned whatever the caller left there. Direct calls
    // from JS usually saw a zeroed $v0 and looked fine; a native->native call
    // left garbage, so every false compare read as true.
    if (isLess) {
        ok("8.9 bool called directly (true)", isLess(1.0f, 2.0f) === true);
        ok("8.10 bool called directly (false)", isLess(2.0f, 1.0f) === false);

        const boolBranch = compile("bool via branch", { args: F2, returns: "float" },
            function (a, b) { if (isLess(a, b)) return 111.0f; return -1.0f; });
        if (boolBranch) {
            eq("8.11 [reg] nested bool true", boolBranch(1.0f, 2.0f), 111.0);
            eq("8.12 [reg] nested bool FALSE", boolBranch(2.0f, 1.0f), -1.0);
        }

        const boolStored = compile("bool stored", { args: F2, returns: "float" },
            function (a, b) {
                const r = isLess(a, b);
                const dummy = a + b;
                if (r) return 222.0f;
                return -2.0f;
            });
        if (boolStored) {
            eq("8.13 [reg] nested bool in a local (true)", boolStored(1.0f, 2.0f), 222.0);
            eq("8.14 [reg] nested bool in a local (false)", boolStored(2.0f, 1.0f), -2.0);
        }

        const boolLoop = compile("bool in loop", { args: F2, returns: "float" },
            function (a, b) {
                for (let i = 0; i < 3; i++) { if (isLess(a, b)) return 333.0f; }
                return -3.0f;
            });
        if (boolLoop) {
            eq("8.15 [reg] nested bool in a loop (true)", boolLoop(1.0f, 2.0f), 333.0);
            eq("8.16 [reg] nested bool in a loop (false)", boolLoop(2.0f, 1.0f), -3.0);
        }
    }

    // Loop + ternary merge + two sequential nested calls - the shape that took
    // six separate compiler bugs to get right.
    if (addTwo && isLess) {
        const combo = compile("combo", { args: F3, returns: "float" },
            function (a, b, lim) {
                const steps = 4;
                const stepsF = (a - a) + 4.0f;
                let acc = 0.0f;
                for (let s = 0; s <= steps; s++) {
                    const t = (s === steps) ? b : addTwo(a, s / stepsF);
                    if (isLess(t, lim)) acc = acc + 1.0f;
                }
                return acc;
            });
        if (combo) {
            let want = 0;
            for (let s = 0; s <= 4; s++) {
                const t = (s === 4) ? 10.0 : (1.0 + s / 4.0);
                if (t < 5.0) want += 1;
            }
            eq("8.17 [reg] loop + ternary + two nested calls", combo(1.0f, 10.0f, 5.0f), want);
        }
    }
}

// ---------------------------------------------------------------------------
// 9. Typed arrays (SoA)
// ---------------------------------------------------------------------------
{
    const N = 8;
    const sum = compile("array sum", { args: ["Float32Array", "int"], returns: "float" },
        function (a, n) { let s = 0.0f; for (let i = 0; i < n; i++) s = s + a[i]; return s; });
    if (sum) {
        const a = new Float32Array(N);
        let want = 0;
        for (let i = 0; i < N; i++) { a[i] = i * 1.5; want += a[i]; }
        eq("9.1 Float32Array read", sum(a, N), want);
    }

    const dot = compile("dot product", { args: ["Float32Array", "Float32Array", "int"], returns: "float" },
        function (a, b, n) { let s = 0.0f; for (let i = 0; i < n; i++) s = s + a[i] * b[i]; return s; });
    if (dot) {
        const a = new Float32Array([1.0, 2.0, 3.0, 4.0]);
        const b = new Float32Array([5.0, 6.0, 7.0, 8.0]);
        eq("9.2 dot product (two arrays)", dot(a, b, 4), 1 * 5 + 2 * 6 + 3 * 7 + 4 * 8);
    }

    const fill = compile("array fill", { args: ["Float32Array", "int"], returns: "void" },
        function (a, n) { for (let i = 0; i < n; i++) a[i] = i * 2.5f; });
    if (fill) {
        const got = new Float32Array(N), want = new Float32Array(N);
        for (let i = 0; i < N; i++) want[i] = i * 2.5;
        fill(got, N);
        eqArr("9.3 Float32Array write", got, want);
    }

    const rmw = compile("array rmw", { args: ["Float32Array", "int"], returns: "void" },
        function (a, n) { for (let i = 0; i < n; i++) a[i] = a[i] * 2.0f + 1.0f; });
    if (rmw) {
        const got = new Float32Array(N), want = new Float32Array(N);
        for (let i = 0; i < N; i++) { got[i] = i * 0.25; want[i] = i * 0.25 * 2 + 1; }
        rmw(got, N);
        eqArr("9.4 read-modify-write", got, want);
    }

    const strided = compile("array strided", { args: ["Float32Array", "Float32Array", "int"], returns: "void" },
        function (pos, vel, n) {
            for (let i = 0; i < n; i++) {
                const b = i * 3;
                pos[b] = pos[b] + vel[b];
                pos[b + 1] = pos[b + 1] + vel[b + 1];
                pos[b + 2] = pos[b + 2] + vel[b + 2];
            }
        });
    if (strided) {
        const P = 4;
        const pos = new Float32Array(P * 3), vel = new Float32Array(P * 3), want = new Float32Array(P * 3);
        for (let i = 0; i < P * 3; i++) { pos[i] = i * 0.5; vel[i] = i * 0.1; want[i] = i * 0.5 + i * 0.1; }
        strided(pos, vel, P);
        eqArr("9.5 strided access", pos, want);
    }

    const intArr = compile("int array", { args: ["Int32Array", "int"], returns: "int" },
        function (a, n) { let s = 0; for (let i = 0; i < n; i++) s = s + a[i]; return s; });
    if (intArr) {
        const a = new Int32Array(N);
        let want = 0;
        for (let i = 0; i < N; i++) { a[i] = i * 3; want += a[i]; }
        eqi("9.6 Int32Array read", intArr(a, N), want);
    }

    const byKind = compile("branch by kind", { args: ["Float32Array", "Int32Array", "int"], returns: "void" },
        function (a, kind, n) {
            for (let i = 0; i < n; i++) {
                const k = kind[i];
                if (k === 0) a[i] = a[i] * 0.5f;
                else if (k === 1) a[i] = a[i] * 2.0f;
                else a[i] = a[i] + 100.0f;
            }
        });
    if (byKind) {
        const got = new Float32Array(N), want = new Float32Array(N);
        const kind = new Int32Array(N);
        for (let i = 0; i < N; i++) {
            got[i] = i + 1.0; kind[i] = i % 3;
            const v = i + 1.0;
            want[i] = kind[i] === 0 ? v * 0.5 : (kind[i] === 1 ? v * 2.0 : v + 100.0);
        }
        byKind(got, kind, N);
        eqArr("9.7 branch on Int32Array", got, want);
    }

    const skip = compile("array continue", { args: ["Float32Array", "Float32Array", "int"], returns: "void" },
        function (life, a, n) {
            for (let i = 0; i < n; i++) {
                if (life[i] <= 0.0f) continue;
                a[i] = a[i] + 10.0f;
            }
        });
    if (skip) {
        const life = new Float32Array(N), got = new Float32Array(N), want = new Float32Array(N);
        for (let i = 0; i < N; i++) {
            life[i] = (i % 2 === 0) ? 1.0 : 0.0;
            got[i] = i;
            want[i] = (i % 2 === 0) ? i + 10 : i;
        }
        skip(life, got, N);
        eqArr("9.8 continue over an array", got, want);
    }
}

// ---------------------------------------------------------------------------
// 10. Int64
// The R5900 has native DADD/DSUB and shifts, but no DMULT/DDIV - multiply,
// divide and modulo go through software emulation. Since CONFIG_BIGNUM was
// removed, int64 values cross the JS boundary as Numbers (exact below 2^53).
// ---------------------------------------------------------------------------
{
    const I64 = ["int64", "int64"];

    const a64 = compile("int64 add", { args: I64, returns: "int64" }, function (a, b) { return a + b; });
    if (a64) { eqi("10.1 int64 add", a64(100, 200), 300); eqi("10.2 int64 add negative", a64(-500, 300), -200); }

    const s64 = compile("int64 sub", { args: I64, returns: "int64" }, function (a, b) { return a - b; });
    if (s64) { eqi("10.3 int64 sub", s64(1000, 300), 700); eqi("10.4 int64 sub negative", s64(100, 500), -400); }

    const n64 = compile("int64 neg", { args: ["int64"], returns: "int64" }, function (a) { return -a; });
    if (n64) { eqi("10.5 int64 negate", n64(42), -42); eqi("10.6 int64 negate negative", n64(-123), 123); }

    const sh64 = compile("int64 shl", { args: ["int64", "int"], returns: "int64" }, function (a, b) { return a << b; });
    if (sh64) { eqi("10.7 int64 shift left", sh64(1, 10), 1024); eqi("10.8 int64 shift left 2", sh64(5, 4), 80); }

    const m64 = compile("int64 mult", { args: I64, returns: "int64" }, function (a, b) { return a * b; });
    if (m64) {
        eqi("10.9 int64 multiply (emulated)", m64(1000, 2000), 2000000);
        eqi("10.10 int64 multiply negative", m64(-500, 500), -250000);
    }

    const d64 = compile("int64 div", { args: I64, returns: "int64" }, function (a, b) { return a / b; });
    if (d64) {
        eqi("10.11 int64 divide (emulated)", d64(1000000, 1000), 1000);
        eqi("10.12 int64 divide negative", d64(-10000, 100), -100);
    }

    const mo64 = compile("int64 mod", { args: I64, returns: "int64" }, function (a, b) { return a % b; });
    if (mo64) {
        eqi("10.13 int64 modulo (emulated)", mo64(1000, 7), 6);
        eqi("10.14 int64 modulo negative", mo64(-25, 7), -4);
    }

    const up = compile("i32 to i64", { args: ["int"], returns: "int64" }, function (a) { return a; });
    if (up) { eqi("10.15 int32 -> int64", up(42), 42); eqi("10.16 int32 -> int64 negative", up(-100), -100); }

    const down = compile("i64 to i32", { args: ["int64"], returns: "int" }, function (a) { return a; });
    if (down) eqi("10.17 int64 -> int32", down(12345), 12345);

    const mixed = compile("mixed int", { args: ["int", "int64"], returns: "int64" }, function (a, b) { return a + b; });
    if (mixed) eqi("10.18 int32 promoted to int64", mixed(100, 200), 300);

    const lt64 = compile("int64 lt", { args: I64, returns: "bool" }, function (a, b) { return a < b; });
    const eq64 = compile("int64 eq", { args: I64, returns: "bool" }, function (a, b) { return a === b; });
    const gt64 = compile("int64 gt", { args: I64, returns: "bool" }, function (a, b) { return a > b; });
    if (lt64) { ok("10.19 int64 <", lt64(100, 200) === true); ok("10.20 int64 < negative", lt64(-100, 50) === true); }
    if (eq64) ok("10.21 int64 ===", eq64(500, 500) === true);
    if (gt64) ok("10.22 int64 >", gt64(1000, 999) === true);
}

// ---------------------------------------------------------------------------
// 11. Strings
// ---------------------------------------------------------------------------
{
    const S2 = ["string", "string"];

    const cat = compile("str concat", { args: S2, returns: "string" }, function (a, b) { return a + b; });
    if (cat) {
        eqs("11.1 string concat", cat("Hello, ", "World!"), "Hello, World!");
        eqs("11.2 concat with empty left", cat("", "test"), "test");
        eqs("11.3 concat with empty right", cat("test", ""), "test");
    }

    const lit = compile("str literal", { args: [], returns: "string" },
        function () { return "Hello" + " " + "Native"; });
    if (lit) eqs("11.4 string literals inside compiled code", lit(), "Hello Native");

    const len = compile("str length", { args: ["string"], returns: "int" }, function (s) { return s.length; });
    if (len) {
        eqi("11.5 string length", len("Hello"), 5);
        eqi("11.6 empty string length", len(""), 0);
        eqi("11.7 string length 9", len("AthenaEnv"), 9);
    }

    const slice = compile("str slice", { args: ["string", "int", "int"], returns: "string" },
        function (s, a, b) { return s.slice(a, b); });
    if (slice) {
        eqs("11.8 string slice", slice("Hello", 1, 4), "ell");
        eqs("11.9 string slice from 0", slice("AthenaEnv", 0, 6), "Athena");
    }

    const idx = compile("str indexOf", { args: S2, returns: "int" }, function (s, n) { return s.indexOf(n); });
    if (idx) {
        eqi("11.10 indexOf found", idx("Hello World", "World"), 6);
        eqi("11.11 indexOf first match", idx("abcabc", "bc"), 1);
        eqi("11.12 indexOf missing", idx("abc", "z"), -1);
    }

    const up = compile("str upper", { args: ["string"], returns: "string" }, function (s) { return s.toUpperCase(); });
    if (up) eqs("11.13 toUpperCase", up("Hello"), "HELLO");

    const lo = compile("str lower", { args: ["string"], returns: "string" }, function (s) { return s.toLowerCase(); });
    if (lo) eqs("11.14 toLowerCase", lo("AthenaEnv"), "athenaenv");

    const trim = compile("str trim", { args: ["string"], returns: "string" }, function (s) { return s.trim(); });
    if (trim) eqs("11.15 trim", trim("  hi  "), "hi");

    const rep = compile("str replace", { args: ["string", "string", "string"], returns: "string" },
        function (s, f, r) { return s.replace(f, r); });
    if (rep) eqs("11.16 replace (first match only)", rep("foo-bar-baz", "-", "_"), "foo_bar-baz");

    const seq = compile("str eq", { args: S2, returns: "bool" }, function (a, b) { return a == b; });
    const sne = compile("str ne", { args: S2, returns: "bool" }, function (a, b) { return a != b; });
    if (seq) {
        ok("11.17 string == equal", seq("abc", "abc") === true);
        ok("11.18 string == different", seq("abc", "def") === false);
    }
    if (sne) {
        ok("11.19 string != different", sne("abc", "def") === true);
        ok("11.20 string != equal", sne("abc", "abc") === false);
    }
}

// ---------------------------------------------------------------------------
// 12. Native.struct
// ---------------------------------------------------------------------------
{
    const Vec3 = attempt("struct Vec3", function () {
        return Native.struct({ x: "float", y: "float", z: "float" });
    });
    if (Vec3) {
        eqi("12.1 struct size", Vec3.size, 12);
        const p = new Vec3();
        p.x = 1.5; p.y = 2.5; p.z = 3.5;
        eq("12.2 struct field x", p.x, 1.5);
        eq("12.3 struct field y", p.y, 2.5);
        eq("12.4 struct field z", p.z, 3.5);
    }

    const Entity = attempt("struct Entity", function () {
        return Native.struct({ id: "int", health: "float", flags: "uint" });
    });
    if (Entity) {
        eqi("12.5 mixed-type struct size", Entity.size, 12);
        const e = new Entity();
        e.id = 42; e.health = 100.5; e.flags = 0xFF00FF00;
        eqi("12.6 struct int field", e.id, 42);
        eq("12.7 struct float field", e.health, 100.5);
        eqi("12.8 struct uint field", e.flags, 4278255360);
    }

    const Matrix4 = attempt("struct Matrix4", function () {
        return Native.struct({ m: "float[16]" });
    });
    if (Matrix4) {
        eqi("12.9 array-field struct size", Matrix4.size, 64);
        const mat = new Matrix4();
        mat.m[0] = 1.0; mat.m[5] = 1.0; mat.m[12] = 10.5; mat.m[13] = 20.5;
        eq("12.10 struct array element [0]", mat.m[0], 1.0);
        eq("12.11 struct array element [12]", mat.m[12], 10.5);
        eq("12.12 struct array element [13]", mat.m[13], 20.5);
        eqi("12.13 struct array length", mat.m.length, 16);

        const sumFirst4 = compile("struct array read", { args: [Matrix4], returns: "float" },
            function (m) {
                let s = 0.0f;
                s = s + m.m[0];
                s = s + m.m[1];
                s = s + m.m[2];
                s = s + m.m[3];
                return s;
            });
        if (sumFirst4) {
            mat.m[0] = 1.0; mat.m[1] = 2.0; mat.m[2] = 3.0; mat.m[3] = 4.0;
            eq("12.14 read a struct array from native code", sumFirst4(mat), 10.0);
        }

        const clearFirst4 = compile("struct array write", { args: [Matrix4], returns: "void" },
            function (m) {
                m.m[0] = 0.0f;
                m.m[1] = 0.0f;
                m.m[2] = 0.0f;
                m.m[3] = 0.0f;
            });
        if (clearFirst4) {
            clearFirst4(mat);
            eq("12.15 write a struct array from native code", mat.m[0], 0.0);
        }
    }

    const IntArr = attempt("struct int array", function () {
        return Native.struct({ data: "int[4]" });
    });
    if (IntArr) {
        const ia = new IntArr();
        ia.data[0] = 100; ia.data[2] = 300;
        eqi("12.16 struct int-array element", ia.data[2], 300);
    }

    const Vec2 = attempt("struct Vec2", function () {
        return Native.struct({ x: "float", y: "float" });
    });
    if (Vec2) {
        const addVec2 = compile("struct args", { args: [Vec2, Vec2, Vec2], returns: "void" },
            function (a, b, out) {
                out.x = a.x + b.x;
                out.y = a.y + b.y;
            });
        if (addVec2) {
            const v1 = new Vec2(), v2 = new Vec2(), vo = new Vec2();
            v1.x = 1.5; v1.y = 2.5;
            v2.x = 3.5; v2.y = 4.5;
            vo.x = 0; vo.y = 0;
            addVec2(v1, v2, vo);
            eq("12.17 struct as a native argument (x)", vo.x, 5.0);
            eq("12.18 struct as a native argument (y)", vo.y, 7.0);
        }
    }

    // Struct methods: 'self' defers compilation until the type is known.
    const Vec3M = attempt("struct with methods", function () {
        return Native.struct({ x: "float", y: "float", z: "float" }, {
            scale: Native.compile({ args: ["self", "float"], returns: "void" },
                function (self, f) {
                    self.x = self.x * f;
                    self.y = self.y * f;
                    self.z = self.z * f;
                }),
            zero: Native.compile({ args: ["self"], returns: "void" },
                function (self) {
                    self.x = 0.0f;
                    self.y = 0.0f;
                    self.z = 0.0f;
                }),
            // Plain JS method alongside the compiled ones
            sum: function () { return this.x + this.y + this.z; },
        });
    });
    if (Vec3M) {
        const v = new Vec3M();
        v.x = 1.0; v.y = 2.0; v.z = 3.0;
        attempt("struct method scale", function () { v.scale(2.0); return 1; });
        eq("12.19 native struct method scale (x)", v.x, 2.0);
        eq("12.20 native struct method scale (y)", v.y, 4.0);
        eq("12.21 native struct method scale (z)", v.z, 6.0);

        const vz = new Vec3M();
        vz.x = 10.0; vz.y = 20.0; vz.z = 30.0;
        attempt("struct method zero", function () { vz.zero(); return 1; });
        eq("12.22 native struct method zero", vz.x + vz.y + vz.z, 0.0);

        const vs = new Vec3M();
        vs.x = 1.5; vs.y = 2.5; vs.z = 3.5;
        eq("12.23 JS method alongside native ones", vs.sum(), 7.5);
    }
}

// ---------------------------------------------------------------------------
// 13. Dynamic arrays
// ---------------------------------------------------------------------------
{
    const pushLen = compile("dyn push", { args: ["DynamicInt32Array", "int"], returns: "int" },
        function (arr, v) { arr.push(v); return arr.length; });

    const getSet = compile("dyn get/set", { args: ["DynamicInt32Array", "int", "int"], returns: "int" },
        function (arr, i, v) { arr[i] = v; return arr[i]; });

    const popOne = compile("dyn pop", { args: ["DynamicInt32Array"], returns: "int" },
        function (arr) { return arr.pop(); });

    const fPush = compile("dyn float push", { args: ["DynamicFloat32Array", "float"], returns: "int" },
        function (arr, v) { arr.push(v); return arr.length; });

    const ia = attempt("createDynamicArray int", function () { return Native.createDynamicArray("int", 8); });
    if (ia && pushLen && getSet && popOne) {
        eqi("13.1 dynamic push -> length 1", pushLen(ia, 10), 1);
        eqi("13.2 dynamic push -> length 2", pushLen(ia, 20), 2);
        eqi("13.3 dynamic push -> length 3", pushLen(ia, 30), 3);
        eqi("13.4 dynamic set/get", getSet(ia, 1, 99), 99);
        eqi("13.5 dynamic element read back from JS", Native.dynArrayGet(ia, 1), 99);
        eqi("13.6 dynamic pop", popOne(ia), 30);
        eqi("13.7 dynamic length after pop", Native.dynArrayLength(ia), 2);
        attempt("dynArrayFree int", function () { Native.dynArrayFree(ia); return 1; });
    }

    const fa = attempt("createDynamicArray float", function () { return Native.createDynamicArray("float", 4); });
    if (fa && fPush) {
        eqi("13.8 dynamic float push -> length 1", fPush(fa, 1.5), 1);
        eqi("13.9 dynamic float push -> length 2", fPush(fa, 2.5), 2);
        eq("13.10 dynamic float element", Native.dynArrayGet(fa, 0), 1.5);
        attempt("dynArrayFree float", function () { Native.dynArrayFree(fa); return 1; });
    }
}

// ---------------------------------------------------------------------------
// 14. float32 x float64 interop at runtime  [reg]
// `f` literals are float32; anything returning a plain double (native call
// results, Math.*, JSON) is float64. js_relational_slow had no branch for the
// mixed pair, so it fell through to the bigint path and compared the float's
// raw bit pattern as an integer: `2.44 <= 0.072f` came out true.
// ---------------------------------------------------------------------------
{
    const f32 = 0.30f * 0.30f * 0.8f;   // float32  (0.072)
    const f64 = 0.30 * 0.30 * 0.8;      // float64  (0.072)
    const big = 2.4477579593658447;     // float64

    ok("14.1 [reg] f64 <= f64", (big <= f64) === false);
    ok("14.2 [reg] f64 <= f32", (big <= f32) === false);
    ok("14.3 [reg] f32 < f64", (f32 < big) === true);
    ok("14.4 [reg] f64 > f32", (big > f32) === true);
    ok("14.5 [reg] f32 >= f64", (f32 >= big) === false);
    eq("14.6 f32 and f64 agree on the value", f32, f64);

    const one = compile("identity", { args: F1, returns: "float" }, function (a) { return a; });
    if (one) {
        ok("14.7 [reg] native return <= f literal", (one(2.0f) <= 0.5f) === false);
        ok("14.8 [reg] native return > f literal", (one(2.0f) > 0.5f) === true);
    }

    // js_strict_eq2: int vs float32 used to return FALSE outright.
    ok("14.9 [reg] 5 === 5.0f", (5 === 5.0f) === true);

    // js_add_slow narrowed any mixed pair to float32, so int + double lost
    // precision and an int+int overflow stopped widening to double. Note the
    // file carries two copies of that function - only the non-CONFIG_BIGNUM one
    // is live now that bignum is gone.
    eq("14.10 [reg] int + double keeps precision", 1 + 0.1, 1.1, 1e-12);
    eq("14.11 [reg] int overflow widens to an exact double", 2147483647 + 1, 2147483648, 0);
}

// ---------------------------------------------------------------------------
// 15. int <-> float conversions
// ---------------------------------------------------------------------------
{
    const i2f = compile("int to float", { args: ["int"], returns: "float" },
        function (a) { return a * 0.5f; });
    if (i2f) eq("15.1 int argument in float arithmetic", i2f(7), 3.5);

    const f2i = compile("float to int", { args: F1, returns: "int" },
        function (a) { return a; });
    if (f2i) {
        eqi("15.2 [reg] float truncated on an int return", f2i(3.9f), 3);
        eqi("15.3 [reg] negative float truncated toward zero", f2i(-3.9f), -3);
    }

    const counter = compile("counter over float", { args: ["int", "float"], returns: "float" },
        function (n, d) {
            let acc = 0.0f;
            for (let i = 0; i < n; i++) acc = acc + i / d;
            return acc;
        });
    if (counter) {
        let want = 0;
        for (let i = 0; i < 4; i++) want += i / 2.0;
        eq("15.4 int counter divided by a float", counter(4, 2.0f), want);
    }
}

// ---------------------------------------------------------------------------
// 16. Introspection API
// ---------------------------------------------------------------------------
{
    const simple = compile("disasm target", { args: F2, returns: "float" }, function (a, b) { return a + b; });
    if (simple) {
        const text = attempt("Native.disassemble", function () { return Native.disassemble(simple); });
        ok("16.1 disassemble returns a non-empty string",
            typeof text === "string" && text.length > 0,
            `type=${typeof text} len=${text ? text.length : 0}`);
    }
    ok("16.2 Native.isSupported()", Native.isSupported() === true);
}

// ---------------------------------------------------------------------------
// 17. Struct arrays (StructType.array(N))
// A contiguous run of N struct instances passed into Native.compile as
// args: [[StructType], "int"] - arr[i].field reads/writes go through
// IR_ARRAY_ELEM_ADDR + IR_LOAD_FIELD_DYN/IR_STORE_FIELD_DYN, which resolve
// the struct base pointer from a genuine runtime value on the eval stack
// instead of a fixed local/argument slot (see IR_LOAD_FIELD/IR_STORE_FIELD,
// which can only ever address a single struct passed directly as an arg).
// ---------------------------------------------------------------------------
{
    const Vec = attempt("struct Vec (array)", function () {
        return Native.struct({ x: "float", y: "float" });
    });

    if (Vec) {
        const va1 = attempt("Vec.array(4) #1", function () { return Vec.array(4); });
        if (va1) {
            va1[2].x = 5.5;
            const readAt2 = compile("struct array read[2]", { args: [[Vec], "int"], returns: "float" },
                function (arr, n) { return arr[2].x; });
            if (readAt2) eq("17.1 struct array read (JS write -> native read)", readAt2(va1, 4), 5.5);
        }

        const va2 = attempt("Vec.array(4) #2", function () { return Vec.array(4); });
        if (va2) {
            const writeAt = compile("struct array write[1]", { args: [[Vec], "int"], returns: "void" },
                function (arr, n) { arr[1].y = 9.5f; });
            if (writeAt) {
                writeAt(va2, 4);
                eq("17.2 struct array write (native write -> JS read)", va2[1].y, 9.5);
            }
        }

        // The main target use case: one native call looping over N struct
        // elements, instead of N separate JS<->native crossings.
        const va3 = attempt("Vec.array(4) #3", function () { return Vec.array(4); });
        if (va3) {
            for (let i = 0; i < 4; i++) { va3[i].x = i * 1.0; va3[i].y = i * 2.0; }
            const sumLoop = compile("struct array loop sum", { args: [[Vec], "int"], returns: "float" },
                function (arr, n) {
                    let s = 0.0f;
                    for (let i = 0; i < n; i++) s = s + arr[i].x + arr[i].y;
                    return s;
                });
            if (sumLoop) {
                let want = 0;
                for (let i = 0; i < 4; i++) want += i * 1.0 + i * 2.0;
                eq("17.3 struct array loop + accumulator", sumLoop(va3, 4), want);
            }
        }

        const va4 = attempt("Vec.array(1) boundary", function () { return Vec.array(1); });
        if (va4) {
            va4[0].x = 42.0;
            const boundaryLoop = compile("struct array boundary loop", { args: [[Vec], "int"], returns: "float" },
                function (arr, n) {
                    let s = 0.0f;
                    for (let i = 0; i < n; i++) s = s + arr[i].x;
                    return s;
                });
            if (boundaryLoop) {
                eq("17.4 struct array loop, zero trips (n=0)", boundaryLoop(va4, 0), 0.0);
                eq("17.5 struct array loop, one trip (n=1)", boundaryLoop(va4, 1), 42.0);
            }
        }

        // [reg] Whole-number float literals reach the compiler as the same
        // compact integer-push opcode as a plain int (section 2 above) -
        // exercise that path through a dynamically-indexed struct field too.
        const va5 = attempt("Vec.array(3) whole literal", function () { return Vec.array(3); });
        if (va5) {
            const wholeWrite = compile("struct array whole literal write", { args: [[Vec], "int"], returns: "void" },
                function (arr, n) {
                    for (let i = 0; i < n; i++) arr[i].x = 5.0f;
                });
            if (wholeWrite) {
                wholeWrite(va5, 3);
                eq("17.6 [reg] whole-number float literal through arr[i].field", va5[1].x, 5.0);
            }
        }

        // [reg] arr[i].x = arr[i].x + arr[i].vx: the RHS re-reads the same
        // index (via its own IR_ARRAY_ELEM_ADDR calls) before the LHS write
        // consumes the original element address. IR_STORE_ARRAY needed an
        // explicit reload fallback for exactly this shape with scalar
        // typed arrays (result[i] = a[i] + b[i]); this is the struct-field
        // analogue, and the highest-risk case for this feature.
        const Vec2 = attempt("struct Vec2 (velocity)", function () {
            return Native.struct({ x: "float", vx: "float" });
        });
        if (Vec2) {
            const va6 = attempt("Vec2.array(3)", function () { return Vec2.array(3); });
            if (va6) {
                for (let i = 0; i < 3; i++) { va6[i].x = i * 1.0; va6[i].vx = 0.5; }
                const selfUpdate = compile("struct array self-referencing write",
                    { args: [[Vec2], "int"], returns: "void" },
                    function (arr, n) {
                        for (let i = 0; i < n; i++) arr[i].x = arr[i].x + arr[i].vx;
                    });
                if (selfUpdate) {
                    selfUpdate(va6, 3);
                    eq("17.7 [reg] arr[i].x = arr[i].x + arr[i].vx", va6[2].x, 2.0 + 0.5);
                }
            }
        }

        // [reg] Reproduces starfox.js's bullet-update shape: a struct mixing
        // FIVE float fields with ONE int field (every prior struct-array case
        // above is float-only), array-length 40 (larger than anything above),
        // and a loop body touching the same element FIVE separate times
        // (read, write, write, read again for the guard, conditional write) -
        // more struct-array field ops per iteration than any case above.
        // Caught a real crash: starfox.js reported "Address Error" at tiny
        // addresses (0x15/0x16) after a few seconds of live bullets, not
        // reproduced by any single-field-op-per-iteration case.
        const N8 = 40;
        const Bullet = attempt("struct Bullet (mixed float/int)", function () {
            return Native.struct({ x: "float", y: "float", z: "float", pz: "float", damage: "float", alive: "int" });
        });
        if (Bullet) {
            const va8 = attempt("Bullet.array(40)", function () { return Bullet.array(N8); });
            if (va8) {
                for (let i = 0; i < N8; i++) {
                    va8[i].x = i * 1.0f; va8[i].y = 0.0f; va8[i].z = 100.0f + i;
                    va8[i].pz = 100.0f + i; va8[i].damage = 1.0f; va8[i].alive = 1;
                }
                const stepBullets8 = compile("struct array bullet step", { args: [[Bullet], "int"], returns: "void" },
                    function (arr, n) {
                        for (let i = 0; i < n; i++) {
                            if (arr[i].alive === 0) continue;
                            arr[i].pz = arr[i].z;
                            arr[i].z = arr[i].z + 3.4f;
                            if (arr[i].z > 135.0f) arr[i].alive = 0;
                        }
                    });
                if (stepBullets8) {
                    // Several frames, same as a real game loop, so any
                    // accumulated-state bug (not just a first-call one) shows up.
                    for (let frame = 0; frame < 20; frame++) stepBullets8(va8, N8);
                    let allOk = true, sample = -1;
                    for (let i = 0; i < N8; i++) {
                        // Don't try to match an exact step count or overshoot
                        // bound - both depend on how many 3.4 increments run
                        // before crossing 135, which varies per i (some start
                        // past 135 already, at i=35+, since z0 = 100+i), and
                        // float32 vs double accumulate differently besides.
                        // Just check the loop's actual invariant: alive===0
                        // iff z ran past 135, and z landed somewhere a real
                        // sequence of +3.4 steps from 100+i could reach (i.e.
                        // not corrupted into a garbage value).
                        const z = va8[i].z, alive = va8[i].alive;
                        const wantAlive = z > 135.0 ? 0 : 1;
                        if (alive !== wantAlive || z < 100.0 + i - 0.01 || z > 100.0 + i + 20 * 3.4 + 0.01) {
                            allOk = false; sample = i; break;
                        }
                    }
                    ok("17.8 [reg] mixed float/int struct array, N=40, 20 frames", allOk,
                        sample >= 0 ? `first mismatch at i=${sample}: z=${va8[sample].z}, alive=${va8[sample].alive}` : undefined);

                    // Plain-JS arr[i].field traffic (not Native.compile), a handful
                    // of "frames" - correctness check only, NOT a stress/timing
                    // test (see the file-level note: wall-clock loops make this
                    // suite flaky). Each arr[i] access allocates a fresh element-
                    // view JSObject (see js_struct_instance_array_get_own_property),
                    // so this same access pattern at real game scale (syncScene/
                    // renderScene read bulletData[i].x/y/z every alive bullet,
                    // every frame) is expensive - that's a starfox.js perf/design
                    // question, not something to chase inside the regression suite.
                    for (let i = 0; i < N8; i++) { va8[i].alive = 1; va8[i].x = i * 1.0f; va8[i].y = i * 2.0f; va8[i].z = i * 3.0f; }
                    let expected = 0.0;
                    for (let i = 0; i < N8; i++) expected += i * 1.0 + i * 2.0 + i * 3.0;
                    let sum = 0.0;
                    for (let frame = 0; frame < 3; frame++) {
                        sum = 0.0;
                        for (let i = 0; i < N8; i++) {
                            if (va8[i].alive !== 0) sum += va8[i].x + va8[i].y + va8[i].z;
                        }
                    }
                    eq("17.9 [reg] plain-JS struct array field access", sum, expected, 0.01);
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Summary
// ---------------------------------------------------------------------------
console.log("");
console.log("========================================");

if (failures.length === 0) {
    console.log(`[smoke] ${passed} passed, 0 failed  -- OK`);
} else {
    console.log(`[smoke] ${passed} passed, ${failures.length} FAILED`);
    for (let i = 0; i < failures.length; i++) console.log(`[smoke]   FAIL ${failures[i]}`);
}
console.log("========================================");

// Show the result on screen too, so the suite can be run from the shell.
const term = new Terminal({ title: "Native compiler smoke test", category: "system",
                            subtitle: failures.length ? `${failures.length} FAILED` : "All cases passed" });
term.log(`${passed} passed, ${failures.length} failed`);
failures.forEach(f => term.log(`FAIL ${f}`));
const pad = Pads.get();
while (true) {
    pad.update();
    if (term.homeRequested(pad)) break;
    term.draw();
}
exitToShell();
