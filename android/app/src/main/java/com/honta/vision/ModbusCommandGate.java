package com.honta.vision;

public final class ModbusCommandGate {
    private int lastHandledCommandId = -1;
    private boolean coldStart = true;

    public Decision evaluate(int commandId, int targetNo, boolean calibrationValid, boolean amrIdle) {
        if (coldStart) {
            coldStart = false;
            lastHandledCommandId = commandId;
            return Decision.ignore("cold start: waiting for the next command id");
        }
        if (commandId == lastHandledCommandId) {
            return Decision.ignore("duplicate command id: " + commandId);
        }
        if (!calibrationValid) {
            lastHandledCommandId = commandId;
            return Decision.reject(commandId, "calibration is not valid");
        }
        if (!amrIdle) {
            lastHandledCommandId = commandId;
            return Decision.reject(commandId, "AMR is not idle");
        }
        if (targetNo <= 0) {
            lastHandledCommandId = commandId;
            return Decision.reject(commandId, "target_no must be positive");
        }
        lastHandledCommandId = commandId;
        return Decision.accept(commandId, targetNo);
    }

    public int lastHandledCommandId() {
        return lastHandledCommandId;
    }

    public static final class Decision {
        public final boolean accepted;
        public final boolean rejected;
        public final int commandId;
        public final int targetNo;
        public final String reason;

        private Decision(
                boolean accepted,
                boolean rejected,
                int commandId,
                int targetNo,
                String reason) {
            this.accepted = accepted;
            this.rejected = rejected;
            this.commandId = commandId;
            this.targetNo = targetNo;
            this.reason = reason;
        }

        public static Decision accept(int commandId, int targetNo) {
            return new Decision(true, false, commandId, targetNo, "");
        }

        public static Decision reject(int commandId, String reason) {
            return new Decision(false, true, commandId, -1, reason);
        }

        public static Decision ignore(String reason) {
            return new Decision(false, false, -1, -1, reason);
        }
    }
}
