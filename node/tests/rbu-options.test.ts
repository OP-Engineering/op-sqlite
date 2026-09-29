import { RBUError, validateRBUOptions } from "../../src/rbu";

describe("RBU option validation", () => {
	test.each([
		undefined,
		{},
		{ targetPath: "", updatePath: "update.sqlite" },
		{ targetPath: "target.sqlite", updatePath: "" },
		{
			targetPath: "/target.sqlite",
			updatePath: "/update.sqlite",
			statePath: "",
		},
		{ targetPath: "target.sqlite", updatePath: "update.sqlite", maxSteps: 0 },
		{ targetPath: "target.sqlite", updatePath: "update.sqlite", maxSteps: 1.5 },
	])("rejects malformed input %#", (options) => {
		expect(() => validateRBUOptions(options as never)).toThrow(RBUError);
	});

	test("accepts bounded and unbounded valid options", () => {
		expect(() =>
			validateRBUOptions({
				targetPath: "/target.sqlite",
				updatePath: "/update.sqlite",
			}),
		).not.toThrow();
		expect(() =>
			validateRBUOptions({
				targetPath: "/target.sqlite",
				updatePath: "/update.sqlite",
				statePath: "/state.sqlite",
				maxSteps: 10,
			}),
		).not.toThrow();
	});
});
