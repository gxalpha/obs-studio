/******************************************************************************
    Copyright (C) 2025 by Sebastian Beckmann

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "moc_OBSSceneCollections.cpp"

#include <obs-frontend-api.h>
#include <QMenu>
#include <QLabel>
#include <QDir>
#include <QShortcut>

#include <Idian/Idian.hpp>
#include <IconLabel.hpp>
#include <dialogs/NameDialog.hpp>
#include <utility/DoubleClickEventFilter.hpp>
#include <qt-wrappers.hpp>
#include <OBSApp.hpp>
#include <models/SceneCollection.hpp>
#include <widgets/OBSBasic.hpp>
#include <importer/OBSImporter.hpp>

OBSSceneCollections::OBSSceneCollections(QWidget *parent) : QDialog(parent), ui(new Ui::OBSSceneCollections)
{
	ui->setupUi(this);

	const char *order = config_get_string(App()->GetUserConfig(), "SceneCollectionsWindow", "Order");
	if (!order)
		order = "LastUsed";

	if (strcmp(order, "Name") == 0) {
		collectionsOrder = SceneCollectionOrder::Name;
	} else {
		collectionsOrder = SceneCollectionOrder::LastUsed;
	}

	refreshList();
	connect(this, &OBSSceneCollections::collectionsChanged, [this]() { refreshList(); });

	QShortcut *shortcut = new QShortcut(QKeySequence("Ctrl+L"), ui->buttonBulkMode);
	connect(shortcut, &QShortcut::activated, ui->buttonBulkMode, &QPushButton::click);

	setAttribute(Qt::WA_DeleteOnClose, true);
}

void OBSSceneCollections::on_lineeditSearch_textChanged(const QString &text)
{
	QString needle = text.toLower();
	for (auto row : ui->group->properties()->rows()) {
		QString name = row->property("name").value<QString>().toLower();
		row->setVisible(name.contains(needle));
	}
}

void OBSSceneCollections::setBulkMode(bool bulk)
{
	if (ui->buttonBulkMode->isChecked() != bulk) {
		// This will run the method again.
		ui->buttonBulkMode->setChecked(bulk);
		return;
	}

	/* Make old buttons invisible first and then new buttons visible to prevent
	 * the window from resizing */
	if (bulk) {
		ui->buttonNew->setVisible(false);
		ui->buttonImport->setVisible(false);
		ui->buttonExportBulk->setVisible(true);
		ui->buttonDuplicateBulk->setVisible(true);
		ui->buttonDeleteBulk->setVisible(true);

		updateBulkButtons();
	} else {
		ui->buttonExportBulk->setVisible(false);
		ui->buttonDuplicateBulk->setVisible(false);
		ui->buttonDeleteBulk->setVisible(false);
		ui->buttonNew->setVisible(true);
		ui->buttonImport->setVisible(true);
	}

	for (auto child : ui->group->properties()->rows()) {
		idian::Row *row = static_cast<idian::Row *>(child);
		if (bulk)
			row->setPrefixEnabled(true);
		else
			row->setSuffixEnabled(true);
	}
}

void OBSSceneCollections::on_buttonBulkMode_toggled(bool checked)
{
	setBulkMode(checked);
}

void OBSSceneCollections::updateBulkButtons()
{
	auto rows = selectedRows();
	if (rows.empty()) {
		ui->buttonExportBulk->setEnabled(false);
		ui->buttonDuplicateBulk->setEnabled(false);
		ui->buttonDeleteBulk->setEnabled(false);
	} else {
		ui->buttonExportBulk->setEnabled(true);
		ui->buttonDuplicateBulk->setEnabled(true);

		bool deleteDisabled = (rows.size() == 1) && rows.front().is_current_collection;
		ui->buttonDeleteBulk->setEnabled(!deleteDisabled);
	}
}

namespace {
// std::chrono Calendar would be so nice. Alas, that's for when we have C++20.
constexpr int SECOND = 1;
constexpr int MINUTE = 60 * SECOND;
constexpr int HOUR = 60 * MINUTE;
constexpr int DAY = 24 * HOUR;
constexpr int WEEK = 7 * DAY;
// A year in the gregorian calendar is, on average, 365.2425 days.
// Therefore a year is 31556952 seconds, and a month 1/12 of that.
// Adding any more code for exact calculations is probably too much effort if std::chrono will solve all of this anyways.
constexpr int MONTH = 2'629'746 * SECOND;
constexpr int YEAR = 31'556'952 * SECOND;

// Upper time limit, Translation key, Translation time divisor
constexpr std::array<std::tuple<int, const char *, int>, 13> timeInfos{{
	{1 * MINUTE, "LastUsed.JustNow", 1},
	{2 * MINUTE, "LastUsed.Minute", MINUTE},
	{1 * HOUR, "LastUsed.Minutes", MINUTE},
	{2 * HOUR, "LastUsed.Hour", HOUR},
	{1 * DAY, "LastUsed.Hours", HOUR},
	{2 * DAY, "LastUsed.Day", DAY},
	{1 * WEEK, "LastUsed.Days", DAY},
	{2 * WEEK, "LastUsed.Week", WEEK},
	{1 * MONTH, "LastUsed.Weeks", WEEK},
	{2 * MONTH, "LastUsed.Month", MONTH},
	{1 * YEAR, "LastUsed.Months", MONTH},
	{2 * YEAR, "LastUsed.Year", YEAR},
	{std::numeric_limits<int>::max(), "LastUsed.Years", YEAR},
}};

QString format_relative_time(std::chrono::time_point<std::chrono::system_clock> from)
{
	auto now = std::chrono::system_clock::now();
	auto difference = now - from;
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(difference).count();

	for (auto &[timeLimit, translationKey, divisor] : timeInfos) {
		if (seconds < timeLimit) {
			return QTStr(translationKey).arg(seconds / divisor);
		}
	}

	return QTStr("Unknown");
}
} // namespace

void OBSSceneCollections::refreshList()
{
	setBulkMode(false);
	ui->buttonDeleteBulk->setEnabled(true);

	ui->group->properties()->clear();

	auto main = OBSBasic::Get();
	auto currentCollection = main->GetCurrentSceneCollection();
	for (auto collectionRef : main->GetSceneCollectionsSorted()) {
		OBS::SceneCollection &collection = collectionRef.get();
		// TODO: Add this information to the SceneCollection object itself
		bool isCurrentCollection = (collection.getName() == currentCollection.getName());

		const QString name = QString::fromStdString(collection.getName());
		const QString description = isCurrentCollection ? QTStr("LastUsed.CurrentlyActive")
								: format_relative_time(collection.getLastUsedTime());
		idian::Row *row = new idian::Row();
		row->setTitle(name);
		row->setDescription(description);
		row->setProperty("name", name);
		row->setProperty("filename", QString::fromStdString(collection.getFileName()));
		row->setProperty("current_collection", isCurrentCollection);

		QCheckBox *checkbox = new QCheckBox(row);
		row->setPrefix(checkbox);
		connect(checkbox, &QCheckBox::toggled, this, &OBSSceneCollections::updateBulkButtons);

		QPushButton *button = new QPushButton(row);
		button->setProperty("class", "icon-dots-vert");
		connect(button, &QPushButton::clicked, this, [this, &collection, isCurrentCollection]() {
			QMenu *menu = new QMenu(this);
			menu->setAttribute(Qt::WA_DeleteOnClose);
			QAction *openAction = menu->addAction(QTStr("SceneCollections.Open"), [&]() {
				OBSBasic::Get()->SetCurrentSceneCollection(collection);
			});
			openAction->setEnabled(!isCurrentCollection);
			menu->addAction(QTStr("SceneCollections.Rename"), []() { /* TODO */ });
			menu->addAction(QTStr("SceneCollections.Duplicate"), []() { /* TODO */ });
			menu->addAction(QTStr("SceneCollections.Export"), [&]() { SCExport(collection); });
			menu->addSeparator();
			QAction *deleteAction = menu->addAction(QTStr("SceneCollections.Delete"), []() { /* TODO */ });
			deleteAction->setEnabled(!isCurrentCollection);
			menu->popup(QCursor::pos());
		});

		QWidget *suffix;
		if (isCurrentCollection) {
			suffix = new QWidget(row);
			QHBoxLayout *layout = new QHBoxLayout(suffix);
			layout->setContentsMargins(0, 0, 0, 0);

			IconLabel *iconWidget = new IconLabel(suffix);
			iconWidget->setProperty("class", "icon-checkmark");

			layout->addWidget(iconWidget);
			button->setParent(suffix);
			layout->addWidget(button);
			suffix->setLayout(layout);
		} else {
			suffix = button;

			DoubleClickEventFilter *filter = new DoubleClickEventFilter(row);
			row->installEventFilter(filter);
			connect(filter, &DoubleClickEventFilter::doubleClicked, this, [&]() {
				if (ui->buttonBulkMode->isChecked())
					return;

				OBSBasic::Get()->SetCurrentSceneCollection(collection);
			});
		}
		row->setSuffix(suffix);
		ui->group->addRow(row);
	}
}

void OBSSceneCollections::on_buttonSort_pressed()
{
	QMenu *menu = new QMenu();
	menu->setAttribute(Qt::WA_DeleteOnClose);
	QAction *lastUsed = menu->addAction(Str("SceneCollections.Sort.LastUsed"), [&]() {
		config_set_string(App()->GetUserConfig(), "SceneCollectionsWindow", "Order", "LastUsed");
		collectionsOrder = SceneCollectionOrder::LastUsed;
		refreshList();
	});
	lastUsed->setCheckable(true);
	lastUsed->setChecked(collectionsOrder == SceneCollectionOrder::LastUsed);
	QAction *name = menu->addAction(Str("SceneCollections.Sort.Name"), [&]() {
		config_set_string(App()->GetUserConfig(), "SceneCollectionsWindow", "Order", "Name");
		collectionsOrder = SceneCollectionOrder::Name;
		refreshList();
	});
	name->setCheckable(true);
	name->setChecked(collectionsOrder == SceneCollectionOrder::Name);
	menu->popup(QCursor::pos());
}

void OBSSceneCollections::on_buttonNew_pressed()
{
	const OBSPromptRequest request{Str("SceneCollections.New.Title"),
				       Str("SceneCollections.GenericNamePrompt.Text")};
	const OBSPromptCallback callback = [](const OBSPromptResult &result) {
		return !OBSBasic::Get()->GetSceneCollectionByName(result.promptValue).has_value();
	};
	const OBSPromptResult result = NameDialog::PromptForName(this, request, callback);

	if (!result.success) {
		return;
	}

	try {
		OBSBasic::Get()->CreateSceneCollection(result.promptValue);
	} catch (const std::invalid_argument &error) {
		blog(LOG_ERROR, "%s", error.what());
	} catch (const std::logic_error &error) {
		blog(LOG_ERROR, "%s", error.what());
	}

	emit collectionsChanged();
}

void OBSSceneCollections::on_buttonImport_pressed()
{
	OBSImporter imp(this);
	imp.exec();

#ifdef __APPLE__
	// TODO: Revisit when QTBUG-42661 is fixed
	raise();
#endif

	emit collectionsChanged();
}

std::vector<OBSSceneCollections::SelectedRowInfo> OBSSceneCollections::selectedRows()
{
	std::vector<SelectedRowInfo> list;
	for (auto child : ui->group->properties()->rows()) {
		idian::Row *row = static_cast<idian::Row *>(child);

		QCheckBox *checkbox = static_cast<QCheckBox *>(row->prefix());
		if (row->isVisible() && checkbox && checkbox->isChecked()) {
			std::string name = row->property("name").toString().toStdString();
			std::string file = row->property("filename").toString().toStdString();
			bool current_collection = row->property("current_collection").toBool();
			// TODO: Figure out emplace_back
			list.push_back({name, file, current_collection});
		}
	}
	return list;
}

void OBSSceneCollections::on_buttonExportBulk_pressed()
{
	auto rows = selectedRows();
	if (rows.empty())
		return;

	if (rows.size() == 1) {
		auto row = rows.front();
		auto collection = OBSBasic::Get()->GetSceneCollectionByName(row.name);
		// TODO: Handling for no value
		SCExport(collection.value());
		return;
	}

	std::string folder =
		SelectDirectory(this, QTStr("SceneCollections.BulkExport.Title"), QDir::homePath()).toStdString();
	if (folder == "")
		return;

	for (auto row : rows) {
		std::string file_name;
		if (!GetFileSafeName(row.name.c_str(), file_name)) {
			blog(LOG_WARNING, "Couldn't generate safe file name for '%s'", file_name.c_str());
			continue;
		}
		std::string export_file = folder + "/" + file_name;
		if (!GetClosestUnusedFileName(export_file, "json")) {
			blog(LOG_WARNING, "Couldn't get closest file name for '%s.json' in '%s'", file_name.c_str(),
			     folder.c_str());
			continue;
		}
		//TODO: ExportSceneCollection(row.file, export_file);
	}

#ifdef __APPLE__
	// TODO: Revisit when QTBUG-42661 is fixed
	raise();
#endif

	refreshList();
}

void OBSSceneCollections::on_buttonDuplicateBulk_pressed()
{
	auto rows = selectedRows();
	if (rows.empty())
		return;

	if (rows.size() == 1) {
		auto row = rows.front();
		SCDuplicate(row.name, row.file);
		return;
	}

	QMessageBox::StandardButton button =
		OBSMessageBox::question(this, QTStr("SceneCollections.BulkDuplicate.Title"),
					QTStr("SceneCollections.BulkDuplicate.Text").arg(rows.size()),
					QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));

	if (button != QMessageBox::Yes)
		return;

	for (auto row : rows) {
		// TODO: This is cursed.
		std::string new_name = row.name;
		do {
			new_name = QTStr("SceneCollections.Duplicate.Default").arg(new_name.c_str()).toStdString();
		} while (OBSBasic::Get()->GetSceneCollectionByName(new_name).has_value());

		//TODO: DuplicateSceneCollection(row.file, new_name);
	}

	emit collectionsChanged();
}

void OBSSceneCollections::on_buttonDeleteBulk_pressed()
{
	auto rows = selectedRows();
	if (rows.empty())
		return;

	if (rows.size() == 1) {
		auto row = rows.front();
		SCDelete(row.name, row.file);
		return;
	}

	for (auto row : rows) {
		if (row.is_current_collection) {
			OBSMessageBox::information(this, QTStr("SceneCollections.BulkDelete.Title"),
						   QTStr("SceneCollections.BulkDelete.ContainsCurrent.Text"));
			return;
		}
	}

	QMessageBox::StandardButton button =
		OBSMessageBox::question(this, QTStr("SceneCollections.BulkDelete.Title"),
					QTStr("SceneCollections.BulkDelete.Text").arg(rows.size()),
					QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));

	if (button != QMessageBox::Yes)
		return;

	BPtr active_collection = obs_frontend_get_current_scene_collection();

	for (auto row : rows) {
		if (row.is_current_collection) {
			blog(LOG_WARNING, "Tried to delete the currently active scene collection. "
					  "This shouldn't be possible.");
			continue;
		}

		//TODO: DeleteSceneCollection(row.file);
	}

	emit collectionsChanged();
}

void OBSSceneCollections::SCRename(const std::string &current_name, const std::string &current_file)
{
	const OBSPromptRequest request{Str("SceneCollections.Rename.Title"),
				       Str("SceneCollections.GenericNamePrompt.Text"), current_name};
	const OBSPromptCallback callback = [](const OBSPromptResult &result) {
		return !OBSBasic::Get()->GetSceneCollectionByName(result.promptValue).has_value();
	};
	const OBSPromptResult result = NameDialog::PromptForName(this, request, callback);
	if (!result.success) {
		return;
	}

	try {
		// TODO: Remove ActivateSceneCollection from that call
		//TODO: The function assumes the current collection.
		//OBSBasic::Get()->SetupRenameSceneCollection(result.promptValue);
	} catch (const std::invalid_argument &error) {
		blog(LOG_ERROR, "%s", error.what());
	} catch (const std::logic_error &error) {
		blog(LOG_ERROR, "%s", error.what());
	}

	emit collectionsChanged();
}

void OBSSceneCollections::SCDuplicate(const std::string &current_name, const std::string &current_file)
{
	const OBSPromptRequest request{
		Str("SceneCollections.Duplicate.Title"), Str("SceneCollections.GenericNamePrompt.Text"),
		QTStr("SceneCollections.Duplicate.Default").arg(current_name.c_str()).toStdString()};
	const OBSPromptCallback callback = [](const OBSPromptResult &result) {
		return !OBSBasic::Get()->GetSceneCollectionByName(result.promptValue).has_value();
	};
	const OBSPromptResult result = NameDialog::PromptForName(this, request, callback);
	if (!result.success) {
		return;
	}

	try {
		// TODO: Remove ActivateSceneCollection from that call
		// TODO: The function assumes the current collection.
		//OBSBasic::Get()->SetupDuplicateSceneCollection(result.promptValue);
	} catch (const std::invalid_argument &error) {
		blog(LOG_ERROR, "%s", error.what());
	} catch (const std::logic_error &error) {
		blog(LOG_ERROR, "%s", error.what());
	}

	emit collectionsChanged();
}

void OBSSceneCollections::SCDelete(const std::string &name, const std::string &file)
{
	auto main = OBSBasic::Get();
	auto currentCollection = main->GetCurrentSceneCollection();
	auto deleteCollectionOpt = main->GetSceneCollectionByName(name);
	if (!deleteCollectionOpt.has_value()) {
		blog(LOG_WARNING, "Tried to delete a scene collection that doesn't exist.");
		return;
	}
	auto deleteCollection = deleteCollectionOpt.value();
	if (currentCollection.getName() == deleteCollection.getName()) {
		blog(LOG_WARNING, "Tried to delete the currently active collection. If this "
				  "message is printed then there very obviously is a bug in the "
				  "program since the menu item should be greyed out for the "
				  "collection that is currently active.");
		return;
	}

	QMessageBox::StandardButton button = OBSMessageBox::question(
		this, QTStr("SceneCollections.Delete.Title"), QTStr("SceneCollections.Delete.Text").arg(name.c_str()),
		QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));

	if (button != QMessageBox::Yes)
		return;

	// TODO: Combine these in RemoveSceneCollections (pass reference into there)
	main->RemoveSceneCollection(deleteCollection);
	main->collections.erase(deleteCollection.getName());

	emit collectionsChanged();
}

void OBSSceneCollections::SCExport(const OBS::SceneCollection &collection)
{
	OBSBasic::Get()->SaveProjectNow();

	const QString home = QDir::homePath();

	const QString destinationFileName = SaveFile(
		this, QTStr("SceneCollections.Export.Title"),
		QDir::homePath() + "/" + QString::fromStdString(collection.getFileName()), "JSON Files (*.json)");

	if (destinationFileName.isEmpty() || destinationFileName.isNull()) {
		return;
	}

	const std::filesystem::path sourceFile = collection.getFilePath();
	const std::filesystem::path destinationFile = std::filesystem::u8path(destinationFileName.toStdString());

	OBSDataAutoRelease collectionData = obs_data_create_from_json_file(sourceFile.u8string().c_str());

	OBSDataArrayAutoRelease sources = obs_data_get_array(collectionData, "sources");
	if (!sources) {
		blog(LOG_WARNING, "No sources in exported scene collection");
		return;
	}

	obs_data_erase(collectionData, "sources");

	std::vector<OBSData> sourceItems;
	obs_data_array_enum(
		sources,
		[](obs_data_t *data, void *vector) -> void {
			auto &sourceItems{*static_cast<std::vector<OBSData> *>(vector)};
			sourceItems.push_back(data);
		},
		&sourceItems);

	std::sort(sourceItems.begin(), sourceItems.end(), [](const OBSData &a, const OBSData &b) {
		return astrcmpi(obs_data_get_string(a, "name"), obs_data_get_string(b, "name")) < 0;
	});

	OBSDataArrayAutoRelease newSources = obs_data_array_create();
	for (auto &item : sourceItems) {
		obs_data_array_push_back(newSources, item);
	}

	obs_data_set_array(collectionData, "sources", newSources);
	obs_data_save_json_pretty_safe(collectionData, destinationFile.u8string().c_str(), "tmp", "bak");

#ifdef __APPLE__
	// TODO: Revisit when QTBUG-42661 is fixed
	raise();
#endif
}
