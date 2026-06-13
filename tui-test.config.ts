import { defineConfig } from "@microsoft/tui-test";

export default defineConfig({
  retries: 1,
  trace: "on-first-retry",
  timeout: 30000,
  expect: {
    timeout: 10000,
  },
  testMatch: "tests/e2e/*.spec.ts",
});
