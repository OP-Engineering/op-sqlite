"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const react_1 = __importStar(require("react"));
const react_native_1 = require("react-native");
require("reflect-metadata");
const Database_1 = require("./Database");
const index_1 = require("./tests/index");
const package_json_1 = __importDefault(require("../package.json"));
const nativewind_1 = require("nativewind");
const react_native_restart_1 = __importDefault(require("react-native-restart"));
const hooks_spec_1 = require("./tests/hooks.spec");
const op_sqlcipher_1 = require("@op-engineering/op-sqlcipher");
const clsx_1 = __importDefault(require("clsx"));
const preparedStatements_spec_1 = require("./tests/preparedStatements.spec");
const constants_spec_1 = require("./tests/constants.spec");
const StyledScrollView = (0, nativewind_1.styled)(react_native_1.ScrollView, {
    props: {
        contentContainerStyle: true,
    },
});
function App() {
    const [isLoading, setIsLoading] = (0, react_1.useState)(false);
    const [results, setResults] = (0, react_1.useState)([]);
    const [times, setTimes] = (0, react_1.useState)([]);
    const [accessingTimes, setAccessingTimes] = (0, react_1.useState)([]);
    const [prepareTimes, setPrepareTimes] = (0, react_1.useState)([]);
    const [prepareExecutionTimes, setPrepareExecutionTimes] = (0, react_1.useState)([]);
    (0, react_1.useEffect)(() => {
        setResults([]);
        (0, index_1.runTests)(index_1.dbSetupTests, index_1.queriesTests, index_1.blobTests, hooks_spec_1.registerHooksTests, preparedStatements_spec_1.preparedStatementsTests, constants_spec_1.constantsTests).then(setResults);
    }, []);
    const createLargeDb = async () => {
        setIsLoading(true);
        await (0, Database_1.createLargeDB)();
        setIsLoading(false);
    };
    const openSampleDB = async () => {
        const sampleDb = (0, op_sqlcipher_1.open)({
            name: 'sampleDB',
        });
    };
    const queryLargeDb = async () => {
        try {
            setIsLoading(true);
            const times = await (0, Database_1.queryLargeDB)();
            setTimes(times.loadFromDb);
            setAccessingTimes(times.access);
            setPrepareTimes(times.prepare);
            setPrepareExecutionTimes(times.preparedExecution);
        }
        catch (e) {
            console.error(e);
        }
        finally {
            setIsLoading(false);
        }
    };
    const queryAndReload = async () => {
        (0, Database_1.queryLargeDB)();
        setTimeout(() => {
            react_native_restart_1.default.restart();
        }, 200);
    };
    const allTestsPassed = results.reduce((acc, r) => {
        return acc && r.type !== 'incorrect';
    }, true);
    const test = () => {
        const testDB = (0, op_sqlcipher_1.open)({
            name: 'testDB',
        });
        // testDB.execute('DROP TABLE IF EXISTS segments;');
        // testDB.execute(
        //   `CREATE TABLE segments ("distance" REAL NOT NULL, "endDate" INTEGER NOT NULL, "id" TEXT PRIMARY KEY, "index" INTEGER NOT NULL, "region" TEXT NOT NULL, "speed" REAL NOT NULL, "startDate" INTEGER NOT NULL, "tripId" TEXT NOT NULL, "startLat" REAL NOT NULL, "startLng" REAL NOT NULL, "endLat" REAL NOT NULL, "endLng" REAL NOT NULL) STRICT;`,
        // );
        const sql = `SELECT EXISTS (
      SELECT 1
      FROM sqlite_master
      WHERE type='table' 
      AND name='your_table_name'
    );
    `;
        testDB.execute(sql);
    };
    return (react_1.default.createElement(react_native_1.SafeAreaView, { className: "flex-1 bg-neutral-900" },
        react_1.default.createElement(StyledScrollView, null,
            react_1.default.createElement(react_native_1.Text, { className: " text-white text-2xl p-2" }, package_json_1.default.name.split('_').join(' ')),
            react_1.default.createElement(react_native_1.View, { className: "flex-row p-2 bg-neutral-800 items-center" },
                react_1.default.createElement(react_native_1.Text, { className: 'font-bold flex-1 text-white' }, "Tools")),
            react_1.default.createElement(react_native_1.Button, { title: "Test", onPress: test }),
            react_1.default.createElement(react_native_1.Button, { title: "Open Sample DB", onPress: openSampleDB }),
            react_1.default.createElement(react_native_1.Button, { title: "Reload app middle of query", onPress: queryAndReload }),
            react_1.default.createElement(react_native_1.Button, { title: "Create 300k Record DB", onPress: createLargeDb }),
            react_1.default.createElement(react_native_1.Button, { title: "Query 300k Records", onPress: queryLargeDb }),
            isLoading && react_1.default.createElement(react_native_1.ActivityIndicator, { color: 'white', size: "large" }),
            !!times.length && (react_1.default.createElement(react_native_1.Text, { className: "text-lg text-white self-center" },
                "Normal query",
                ' ',
                (times.reduce((acc, t) => (acc += t), 0) / times.length).toFixed(0),
                ' ',
                "ms")),
            !!accessingTimes.length && (react_1.default.createElement(react_native_1.Text, { className: "text-lg text-white self-center" },
                "Read property",
                ' ',
                (accessingTimes.reduce((acc, t) => (acc += t), 0) /
                    accessingTimes.length).toFixed(0),
                ' ',
                "ms")),
            !!prepareTimes.length && (react_1.default.createElement(react_native_1.Text, { className: "text-lg text-white self-center" },
                "Prepare statement",
                ' ',
                (prepareTimes.reduce((acc, t) => (acc += t), 0) /
                    prepareTimes.length).toFixed(0),
                ' ',
                "ms")),
            !!prepareExecutionTimes.length && (react_1.default.createElement(react_native_1.Text, { className: "text-lg text-white self-center" },
                "Execute prepared query",
                ' ',
                (prepareExecutionTimes.reduce((acc, t) => (acc += t), 0) /
                    prepareExecutionTimes.length).toFixed(0),
                ' ',
                "ms")),
            react_1.default.createElement(react_native_1.Text, { className: (0, clsx_1.default)('font-bold flex-1 text-white p-2 mt-4', {
                    'bg-green-500': allTestsPassed,
                    'bg-red-500': !allTestsPassed,
                }) }, "Test Suite"),
            results.map((r, i) => {
                if (r.type === 'grouping') {
                    return (react_1.default.createElement(react_native_1.Text, { className: "p-2 font-semibold text-white", key: i }, r.description));
                }
                if (r.type === 'incorrect') {
                    return (react_1.default.createElement(react_native_1.View, { className: "border-b border-neutral-800 p-2 flex-row", key: i },
                        react_1.default.createElement(react_native_1.Text, { className: "text-red-500 flex-1" },
                            r.description,
                            ": ",
                            r.errorMsg)));
                }
                return (react_1.default.createElement(react_native_1.View, { className: "border-b border-neutral-800 p-2 flex-row", key: i },
                    react_1.default.createElement(react_native_1.Text, { className: "text-green-500 flex-1" }, r.description)));
            }))));
}
exports.default = App;
