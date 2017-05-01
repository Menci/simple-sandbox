export interface SandboxParameter {
    // Time limit, in milliseconds.
    time: number;
    memory: number;
    process: number;
    redirectBeforeChroot: boolean;
    mountProc: boolean;
    chroot: string;
    binary?: string;
    working: string;
    executable: string;
    stdin?: string;
    stdout?: string;
    stderr?: string;
    user: string;
    cgroup: string;
    parameters?: string;
    environments?: string;
};

export enum SandboxStatus {
    Unknown = 0,
    OK = 1,
    TimeLimitExceeded = 2,
    MemoryLimitExceeded = 3,
    RuntimeError = 4,
    Cancelled = 5
};

export interface SandboxResult {
    status: SandboxStatus;
    time: number;
    memory: number;
    code: number;
};