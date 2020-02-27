import { parentPort } from 'worker_threads';
import nativeAddon from "./nativeAddon";

parentPort.on("message", (waitParams: { pid: number, execParam: ArrayBuffer }) => {
  parentPort.postMessage(nativeAddon.waitForProcess(waitParams.pid, waitParams.execParam));
  process.exit(0);
});
