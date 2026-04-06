import { openAsync } from "@op-engineering/op-sqlite";
import { useCallback, useEffect, useState } from "react";
import {
	Pressable,
	ScrollView,
	StyleSheet,
	Text,
	TextInput,
	View,
} from "react-native";

type TableColumn = {
	cid: number;
	name: string;
	type: string;
	notnull: number;
	pk: number;
};

type Note = {
	id: number;
	label: string;
	created_at: string;
};

export default function AppWeb() {
	const [status, setStatus] = useState("Initializing sqlite web backend...");
	const [columns, setColumns] = useState<TableColumn[]>([]);
	const [notes, setNotes] = useState<Note[]>([]);
	const [inputValue, setInputValue] = useState("First note from OPFS web db");
	const [loading, setLoading] = useState(false);

	const withDb = useCallback(
		async <T,>(
			work: (db: Awaited<ReturnType<typeof openAsync>>) => Promise<T>,
		) => {
			const db = await openAsync({
				name: "example-web.sqlite",
			});

			try {
				return await work(db);
			} finally {
				await db.closeAsync();
			}
		},
		[],
	);

	const refreshTableInfo = useCallback(async () => {
		return withDb(async (db) => {
			const [schemaResult, rowsResult] = await Promise.all([
				db.execute("PRAGMA table_info(web_notes)"),
				db.execute(
					"SELECT id, label, created_at FROM web_notes ORDER BY id DESC LIMIT 20",
				),
			]);

			setColumns(
				schemaResult.rows.map((row) => ({
					cid: Number(row.cid),
					name: String(row.name),
					type: String(row.type),
					notnull: Number(row.notnull),
					pk: Number(row.pk),
				})),
			);

			setNotes(
				rowsResult.rows.map((row) => ({
					id: Number(row.id),
					label: String(row.label),
					created_at: String(row.created_at),
				})),
			);
		});
	}, [withDb]);

	const ensureSchema = useCallback(async () => {
		return withDb(async (db) => {
			await db.execute(
				"CREATE TABLE IF NOT EXISTS web_notes (id INTEGER PRIMARY KEY AUTOINCREMENT, label TEXT NOT NULL, created_at TEXT NOT NULL)",
			);
		});
	}, [withDb]);

	const insertNote = useCallback(async () => {
		const value = inputValue.trim();
		if (!value) {
			setStatus("Type a value before inserting.");
			return;
		}

		setLoading(true);
		try {
			await withDb(async (db) => {
				await db.execute(
					"INSERT INTO web_notes (label, created_at) VALUES (?, ?)",
					[value, new Date().toISOString()],
				);
			});

			setStatus("Insert succeeded. Data is persisted in OPFS.");
			await refreshTableInfo();
			setInputValue("");
		} catch (error) {
			setStatus(`Insert failed: ${(error as Error).message}`);
		} finally {
			setLoading(false);
		}
	}, [inputValue, refreshTableInfo, withDb]);

	const insertSampleData = useCallback(async () => {
		setLoading(true);
		try {
			await withDb(async (db) => {
				const now = new Date().toISOString();

				await db.execute(
					"INSERT INTO web_notes (label, created_at) VALUES (?, ?)",
					["Sample: OPFS persistence check", now],
				);
				await db.execute(
					"INSERT INTO web_notes (label, created_at) VALUES (?, ?)",
					["Sample: Query path works", now],
				);
				await db.execute(
					"INSERT INTO web_notes (label, created_at) VALUES (?, ?)",
					["Sample: closeAsync completed", now],
				);
			});

			setStatus("Sample rows inserted successfully.");
			await refreshTableInfo();
		} catch (error) {
			setStatus(`Sample insert failed: ${(error as Error).message}`);
		} finally {
			setLoading(false);
		}
	}, [refreshTableInfo, withDb]);

	const clearRows = useCallback(async () => {
		setLoading(true);
		try {
			await withDb(async (db) => {
				await db.execute("DELETE FROM web_notes");
			});

			setStatus("Table cleared.");
			await refreshTableInfo();
		} catch (error) {
			setStatus(`Clear failed: ${(error as Error).message}`);
		} finally {
			setLoading(false);
		}
	}, [refreshTableInfo, withDb]);

	useEffect(() => {
		const work = async () => {
			try {
				await ensureSchema();
				await refreshTableInfo();
				setStatus("SQLite web backend initialized with OPFS persistence.");
			} catch (error) {
				setStatus(
					`Failed to initialize web sqlite backend: ${(error as Error).message}`,
				);
			}
		};

		work();
	}, [ensureSchema, refreshTableInfo]);

	return (
		<ScrollView contentContainerStyle={styles.container}>
			<Text style={styles.title}>OP-SQLite Web Example</Text>
			<Text style={styles.status}>{status}</Text>

			<View style={styles.panel}>
				<Text style={styles.panelTitle}>1) Insert data</Text>
				<TextInput
					style={styles.input}
					placeholder="Type note text"
					placeholderTextColor="#6b7280"
					value={inputValue}
					editable={!loading}
					onChangeText={setInputValue}
				/>
				<View style={styles.row}>
					<Pressable
						style={styles.button}
						disabled={loading}
						onPress={insertNote}
					>
						<Text style={styles.buttonText}>Insert one row</Text>
					</Pressable>
					<Pressable
						style={styles.buttonSecondary}
						disabled={loading}
						onPress={insertSampleData}
					>
						<Text style={styles.buttonText}>Insert sample rows</Text>
					</Pressable>
				</View>
			</View>

			<View style={styles.panel}>
				<Text style={styles.panelTitle}>Table: web_notes</Text>
				<View style={styles.row}>
					<Pressable
						style={styles.button}
						disabled={loading}
						onPress={refreshTableInfo}
					>
						<Text style={styles.buttonText}>Refresh table</Text>
					</Pressable>
					<Pressable
						style={styles.buttonDanger}
						disabled={loading}
						onPress={clearRows}
					>
						<Text style={styles.buttonText}>Clear table</Text>
					</Pressable>
				</View>

				<Text style={styles.sectionLabel}>Schema</Text>
				<View style={styles.tableHeader}>
					<Text style={[styles.headerCell, styles.cidCell]}>cid</Text>
					<Text style={[styles.headerCell, styles.nameCell]}>name</Text>
					<Text style={[styles.headerCell, styles.typeCell]}>type</Text>
					<Text style={[styles.headerCell, styles.flagCell]}>nn</Text>
					<Text style={[styles.headerCell, styles.flagCell]}>pk</Text>
				</View>
				{columns.map((column) => (
					<View key={column.cid} style={styles.tableRow}>
						<Text style={[styles.rowCell, styles.cidCell]}>{column.cid}</Text>
						<Text style={[styles.rowCell, styles.nameCell]}>{column.name}</Text>
						<Text style={[styles.rowCell, styles.typeCell]}>{column.type}</Text>
						<Text style={[styles.rowCell, styles.flagCell]}>
							{column.notnull}
						</Text>
						<Text style={[styles.rowCell, styles.flagCell]}>{column.pk}</Text>
					</View>
				))}

				<Text style={styles.sectionLabel}>Rows ({notes.length})</Text>
				<View style={styles.tableHeader}>
					<Text style={[styles.headerCell, styles.cidCell]}>id</Text>
					<Text style={[styles.headerCell, styles.nameCell]}>label</Text>
					<Text style={[styles.headerCell, styles.dateCell]}>created_at</Text>
				</View>
				{notes.length === 0 ? (
					<Text style={styles.empty}>No rows in web_notes.</Text>
				) : (
					notes.map((entry) => (
						<View key={entry.id} style={styles.tableRow}>
							<Text style={[styles.rowCell, styles.cidCell]}>{entry.id}</Text>
							<Text style={[styles.rowCell, styles.nameCell]}>
								{entry.label}
							</Text>
							<Text style={[styles.rowCell, styles.dateCell]}>
								{entry.created_at}
							</Text>
						</View>
					))
				)}
			</View>
		</ScrollView>
	);
}

const styles = StyleSheet.create({
	container: {
		minHeight: "100%",
		padding: 24,
		backgroundColor: "#101418",
		gap: 12,
	},
	title: {
		color: "#f9fafb",
		fontWeight: "700",
		fontSize: 24,
		marginBottom: 2,
	},
	status: {
		color: "#9ca3af",
		marginBottom: 8,
	},
	panel: {
		borderWidth: 1,
		borderColor: "#273043",
		borderRadius: 12,
		backgroundColor: "#151a22",
		padding: 12,
		gap: 8,
	},
	panelTitle: {
		color: "#f3f4f6",
		fontWeight: "600",
		fontSize: 16,
	},
	sectionLabel: {
		color: "#cbd5e1",
		marginTop: 4,
		marginBottom: 2,
		fontWeight: "600",
	},
	row: {
		flexDirection: "row",
		flexWrap: "wrap",
		gap: 8,
	},
	input: {
		borderWidth: 1,
		borderColor: "#334155",
		borderRadius: 10,
		color: "#f8fafc",
		paddingHorizontal: 10,
		paddingVertical: 9,
		backgroundColor: "#0f172a",
	},
	button: {
		backgroundColor: "#2563eb",
		borderRadius: 8,
		paddingHorizontal: 12,
		paddingVertical: 9,
	},
	buttonSecondary: {
		backgroundColor: "#0ea5e9",
		borderRadius: 8,
		paddingHorizontal: 12,
		paddingVertical: 9,
	},
	buttonDanger: {
		backgroundColor: "#dc2626",
		borderRadius: 8,
		paddingHorizontal: 12,
		paddingVertical: 9,
	},
	buttonText: {
		color: "#f8fafc",
		fontWeight: "600",
	},
	empty: {
		color: "#9ca3af",
		fontStyle: "italic",
	},
	tableHeader: {
		flexDirection: "row",
		borderBottomWidth: 1,
		borderBottomColor: "#334155",
		paddingBottom: 6,
		marginTop: 4,
	},
	tableRow: {
		flexDirection: "row",
		borderBottomWidth: 1,
		borderBottomColor: "#1f2937",
		paddingVertical: 8,
	},
	headerCell: {
		color: "#93c5fd",
		fontWeight: "700",
		fontSize: 12,
	},
	rowCell: {
		color: "#e5e7eb",
		fontSize: 13,
	},
	cidCell: {
		width: 40,
	},
	nameCell: {
		flex: 1,
		paddingRight: 8,
	},
	typeCell: {
		width: 80,
	},
	flagCell: {
		width: 32,
	},
	dateCell: {
		flex: 1,
		paddingLeft: 8,
	},
	item: {
		borderWidth: 1,
		borderColor: "#334155",
		borderRadius: 10,
		padding: 9,
		gap: 2,
		backgroundColor: "#0f172a",
	},
	itemTitle: {
		color: "#93c5fd",
		fontWeight: "700",
	},
	itemText: {
		color: "#f1f5f9",
	},
	itemMeta: {
		color: "#94a3b8",
		fontSize: 14,
	},
});
