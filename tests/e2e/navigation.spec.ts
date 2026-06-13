import { test, expect, Key } from "@microsoft/tui-test";
import { startServer, stopServer } from "./test-server.js";
import { resolve } from "node:path";

const program = { file: resolve("build/fuse-diag") };

test.describe("FuseDiag Navigation", () => {
  test.beforeAll(async () => {
    await startServer();
  });

  test.afterAll(() => {
    stopServer();
  });

  test.use({ program });

  test("nav menu items visible", async ({ terminal }) => {
    await expect(terminal.getByText("Raw Send")).toBeVisible();
    await expect(terminal.getByText("Session")).toBeVisible();
    await expect(terminal.getByText("Settings")).toBeVisible();
  });

  test("Tab cycles through all pages", async ({ terminal }) => {
    terminal.keyPress(Key.Tab);
    terminal.keyPress(Key.Tab);
    terminal.keyPress(Key.Tab);
    terminal.keyPress(Key.Tab);
    terminal.keyPress(Key.Tab);
  });

  test("F2 jumps to DID page", async ({ terminal }) => {
    terminal.keyPress(Key.F2);

    await expect(terminal.getByText("Database")).toBeVisible();
    await expect(terminal.getByText("Browser")).toBeVisible();
  });

  test("F3 shows Raw Send page", async ({ terminal }) => {
    terminal.keyPress(Key.F3);

    await expect(terminal.getByText("Request")).toBeVisible();
  });
});
