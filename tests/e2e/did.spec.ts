import { test, expect, Key } from "@microsoft/tui-test";
import { startServer, stopServer } from "./test-server.js";
import { resolve } from "node:path";

const program = { file: resolve("build/fuse-diag") };

test.describe("FuseDiag DID Page", () => {
  test.beforeAll(async () => {
    await startServer();
  });

  test.afterAll(() => {
    stopServer();
  });

  test.use({ program });

  test("F2 shows Database and Browser tabs", async ({ terminal }) => {
    terminal.keyPress(Key.F2);

    await expect(terminal.getByText("Database")).toBeVisible();
    await expect(terminal.getByText("Browser")).toBeVisible();
    await expect(terminal.getByText("Polling")).toBeVisible();
  });
});
