/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "networkremotesettingspage.h"

#include <QDesktopServices>
#include <QFile>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QSettings>
#include <QUrl>
#include <algorithm>

#include "core/application.h"
#include "networkremote/networkremote.h"
#include "networkremote/networkremotehelper.h"
#include "transcoder/transcoder.h"
#include "transcoder/transcoderoptionsdialog.h"
#include "ui/iconloader.h"
#include "ui/settingsdialog.h"
#include "ui_networkremotesettingspage.h"

const char* NetworkRemoteSettingsPage::kPlayStoreUrl =
    "https://play.google.com/store/apps/details?id=de.qspool.clementineremote";
const char* NetworkRemoteSettingsPage::kPlayStoreUrl2 =
    "https://play.google.com/store/apps/details?id=fr.mbruel.ClementineRemote";
const char* NetworkRemoteSettingsPage::kAppleStoreUrl =
    "https://apps.apple.com/fr/app/clemremote/id1541922045";
const char* NetworkRemoteSettingsPage::kLatestReleasesUrl =
    "https://github.com/mbruel/ClementineRemote/releases/latest";

static bool ComparePresetsByName(const TranscoderPreset& left,
                                 const TranscoderPreset& right) {
  return left.name_ < right.name_;
}

NetworkRemoteSettingsPage::NetworkRemoteSettingsPage(SettingsDialog* dialog)
    : SettingsPage(dialog), ui_(new Ui_NetworkRemoteSettingsPage) {
  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("ipodtouchicon", IconLoader::Base));

  connect(ui_->options, SIGNAL(clicked()), SLOT(Options()));

  ui_->play_store->installEventFilter(this);
  ui_->play_store_2->installEventFilter(this);
  ui_->apple_store->installEventFilter(this);
  ui_->desktop_remote->installEventFilter(this);

  // Get presets
  QList<TranscoderPreset> presets = Transcoder::GetAllPresets();
  std::sort(presets.begin(), presets.end(), ComparePresetsByName);
  for (const TranscoderPreset& preset : presets) {
    ui_->format->addItem(
        QString("%1 (.%2)").arg(preset.name_, preset.extension_),
        QVariant::fromValue(preset));
  }
}

NetworkRemoteSettingsPage::~NetworkRemoteSettingsPage() { delete ui_; }

bool NetworkRemoteSettingsPage::eventFilter(QObject* object, QEvent* event) {
  if (event->type() == QEvent::MouseButtonRelease) {
    QUrl url;
    if (object == ui_->play_store)
      url = QUrl(kPlayStoreUrl);
    else if (object == ui_->play_store_2)
      url = QUrl(kPlayStoreUrl2);
    else if (object == ui_->apple_store)
      url = QUrl(kAppleStoreUrl);
    else if (object == ui_->desktop_remote)
      url = QUrl(kLatestReleasesUrl);

    if (!url.isEmpty()) {
      QDesktopServices::openUrl(url);
      return true;
    }
  }

  return SettingsPage::eventFilter(object, event);
}

void NetworkRemoteSettingsPage::Load() {
  QSettings s;

  s.beginGroup(NetworkRemote::kSettingsGroup);

  ui_->use_remote->setChecked(s.value("use_remote").toBool());
  ui_->remote_port->setValue(
      s.value("port", NetworkRemote::kDefaultServerPort).toInt());
  ui_->only_non_public_ip->setChecked(
      s.value("only_non_public_ip", true).toBool());

  // Auth Code, 5 digits
  ui_->use_auth_code->setChecked(s.value("use_auth_code", false).toBool());
  ui_->auth_code->setValue(s.value("auth_code", qrand() % 100000).toInt());

  ui_->allow_downloads->setChecked(s.value("allow_downloads", false).toBool());
  ui_->convert_lossless->setChecked(
      s.value("convert_lossless", false).toBool());

  // Load settings
  QString last_output_format =
      s.value("last_output_format", "audio/x-vorbis").toString();
  for (int i = 0; i < ui_->format->count(); ++i) {
    if (last_output_format ==
        ui_->format->itemData(i).value<TranscoderPreset>().codec_mimetype_) {
      ui_->format->setCurrentIndex(i);
      break;
    }
  }

  ui_->files_root_folder->SetPath(s.value("files_root_folder", "").toString());
  ui_->files_music_extensions->setText(
      s.value("files_music_extensions",
              Application::kDefaultMusicExtensionsAllowedRemotely)
          .toStringList()
          .join(","));

  s.endGroup();

  // Get local ip addresses
  QString ip_addresses;
  QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
  for (const QHostAddress& address : addresses) {
    // TODO: Add ipv6 support to tinysvcmdns.
    if (address.protocol() == QAbstractSocket::IPv4Protocol &&
        !address.isInSubnet(QHostAddress::parseSubnet("127.0.0.1/8"))) {
      if (!ip_addresses.isEmpty()) {
        ip_addresses.append(", ");
      }
      ip_addresses.append(address.toString());
    }
  }
  ui_->ip_address->setText(ip_addresses);

  // Get the right play store badge for this language.
  QString language = dialog()->app()->language_without_region();

  QString badge_filename = ":/playstore/" + language + "_generic_rgb_wo_45.png";
  if (!QFile::exists(badge_filename)) {
    badge_filename = ":/playstore/en_generic_rgb_wo_45.png";
  }

  ui_->play_store->setPixmap(QPixmap(badge_filename));
  ui_->play_store_2->setPixmap(QPixmap(badge_filename));

  ui_->desktop_remote->setText(
      tr("You can find <a href=\"%1\">here on GitHub</a> the new cross "
         "platform remote.<br/>It is available on <b>Linux</b>, <b>MacOS</b> "
         "and <b>Windows</b><br/>")
          .arg(kLatestReleasesUrl));
  ui_->desktop_remote->setWordWrap(true);
}

void NetworkRemoteSettingsPage::Save() {
  QSettings s;

  s.beginGroup(NetworkRemote::kSettingsGroup);
  s.setValue("port", ui_->remote_port->value());
  s.setValue("use_remote", ui_->use_remote->isChecked());
  s.setValue("only_non_public_ip", ui_->only_non_public_ip->isChecked());
  s.setValue("use_auth_code", ui_->use_auth_code->isChecked());
  s.setValue("auth_code", ui_->auth_code->value());
  s.setValue("allow_downloads", ui_->allow_downloads->isChecked());
  s.setValue("convert_lossless", ui_->convert_lossless->isChecked());

  TranscoderPreset preset = ui_->format->itemData(ui_->format->currentIndex())
                                .value<TranscoderPreset>();
  s.setValue("last_output_format", preset.codec_mimetype_);

  s.setValue("files_root_folder", ui_->files_root_folder->Path());

  QStringList files_music_extensions;
  for (const QString& extension :
       ui_->files_music_extensions->text().split(",")) {
    QString ext = extension.trimmed();
    if (ext.size() > 0 && ext.size() < 8)  // no empty string, less than 8 char
      files_music_extensions << ext;
  }
  s.setValue("files_music_extensions", files_music_extensions);

  s.endGroup();

  if (NetworkRemoteHelper::Instance()) {
    NetworkRemoteHelper::Instance()->ReloadSettings();
  }
}

void NetworkRemoteSettingsPage::Options() {
  TranscoderPreset preset = ui_->format->itemData(ui_->format->currentIndex())
                                .value<TranscoderPreset>();

  TranscoderOptionsDialog dialog(preset.type_, this);
  dialog.set_settings_postfix(NetworkRemote::kTranscoderSettingPostfix);
  if (dialog.is_valid()) {
    dialog.exec();
  }
}
