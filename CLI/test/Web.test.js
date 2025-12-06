// Smoke test for Luau.Web
// Usage: node Web.test.js ./Luau.Web.js

const Module = require(require('path').resolve(process.cwd(), process.argv[2]));

Module.onRuntimeInitialized = () => {
    const exec = Module.cwrap('executeScript', 'string', ['string']);
    const err = exec('print(ZERO_VECTOR)');
    process.exit(err ? 1 : 0);
};
