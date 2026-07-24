#!/usr/bin/env qjs
// Keke OS JavaScript Demo
function fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

function printHeader() {
    console.log("=== Keke OS JavaScript Demo ===");
    console.log("Engine: QuickJS");
    console.log("");
}

function printFooter() {
    console.log("");
    console.log("================================");
}

function main() {
    printHeader();

    console.log("Fibonacci sequence:");
    for (var i = 0; i <= 20; i++) {
        console.log("  fib(" + i + ") = " + fib(i));
    }

    console.log("");
    console.log("Array methods:");
    var nums = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5];
    console.log("  Original: " + nums.join(", "));
    nums.sort(function(a, b) { return a - b; });
    console.log("  Sorted:   " + nums.join(", "));
    console.log("  Sum:      " + nums.reduce(function(a, b) { return a + b; }, 0));

    console.log("");
    console.log("Current time: " + new Date().toISOString());

    printFooter();
}

main();
