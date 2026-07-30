package com.honta.vision;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

public final class AmrPoseValidator {
    private final long maxAgeMillis;
    private final Set<String> allowedFsmStates;

    public AmrPoseValidator(long maxAgeMillis, Set<String> allowedFsmStates) {
        this.maxAgeMillis = maxAgeMillis;
        this.allowedFsmStates = Collections.unmodifiableSet(new HashSet<>(allowedFsmStates));
    }

    public static AmrPoseValidator defaultForCalibration() {
        Set<String> states = new HashSet<>();
        states.add("idle");
        return new AmrPoseValidator(1000L, states);
    }

    public ValidationResult validate(AmrPoseSnapshot pose, long nowMillis) {
        if (pose == null) {
            return ValidationResult.fail("AMR pose is missing");
        }
        long age = nowMillis - pose.timestampMillis;
        if (age < 0 || age > maxAgeMillis) {
            return ValidationResult.fail("AMR pose is stale: age_ms=" + age);
        }
        if (pose.fsm == null || !allowedFsmStates.contains(pose.fsm)) {
            return ValidationResult.fail("AMR FSM is not allowed for calibration: " + pose.fsm);
        }
        if (pose.localizationError) {
            return ValidationResult.fail("AMR localization error is active");
        }
        if (pose.collision) {
            return ValidationResult.fail("AMR collision state is active");
        }
        return ValidationResult.ok();
    }

    public static final class ValidationResult {
        public final boolean ok;
        public final String message;

        private ValidationResult(boolean ok, String message) {
            this.ok = ok;
            this.message = message;
        }

        public static ValidationResult ok() {
            return new ValidationResult(true, "");
        }

        public static ValidationResult fail(String message) {
            return new ValidationResult(false, message);
        }
    }
}
