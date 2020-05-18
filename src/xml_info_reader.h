#ifndef XML_INFO_READER_H
#define XML_INFO_READER_H

#include <QString>
#include <QIODevice>
#include <QXmlStreamReader>
#include <QTextStream>
#include <QFile>
#include <QTextCodec>

#include "utility.h"
#include "log.h"

// This is from installer_fomod, but should probably not be duplicated here.

struct FomodInfoReader: QObject {

  Q_OBJECT

public:

  struct XmlParseError : MOBase::MyException {
    XmlParseError(const QString& message): MyException(message) {}
  };

  static QByteArray skipXmlHeader(QIODevice& file)
  {
    static const unsigned char UTF16LE_BOM[] = { 0xFF, 0xFE };
    static const unsigned char UTF16BE_BOM[] = { 0xFE, 0xFF };
    static const unsigned char UTF8_BOM[] = { 0xEF, 0xBB, 0xBF };
    static const unsigned char UTF16LE[] = { 0x3C, 0x00, 0x3F, 0x00 };
    static const unsigned char UTF16BE[] = { 0x00, 0x3C, 0x00, 0x3F };
    static const unsigned char UTF8[] = { 0x3C, 0x3F, 0x78, 0x6D };

    file.seek(0);
    QByteArray rawBytes = file.read(4);
    QTextStream stream(&file);
    int bom = 0;
    if (rawBytes.startsWith((const char*)UTF16LE_BOM)) {
      stream.setCodec("UTF16-LE");
      bom = 2;
    }
    else if (rawBytes.startsWith((const char*)UTF16BE_BOM)) {
      stream.setCodec("UTF16-BE");
      bom = 2;
    }
    else if (rawBytes.startsWith((const char*)UTF8_BOM)) {
      stream.setCodec("UTF-8");
      bom = 3;
    }
    else if (rawBytes.startsWith(QByteArray((const char*)UTF16LE, 4))) {
      stream.setCodec("UTF16-LE");
    }
    else if (rawBytes.startsWith(QByteArray((const char*)UTF16BE, 4))) {
      stream.setCodec("UTF16-BE");
    }
    else if (rawBytes.startsWith(QByteArray((const char*)UTF8, 4))) {
      stream.setCodec("UTF-8");
    } // otherwise maybe the textstream knows the encoding?

    stream.seek(bom);
    QString header = stream.readLine();
    if (!header.startsWith("<?")) {
      // it was all for nothing, there is no header here...
      stream.seek(bom);
    }
    // this seems to be necessary due to buffering in QTextStream
    file.seek(stream.pos());
    return file.readAll();
  }

  template <class Fn>
  static auto readXml(QFile& file, Fn &&fn)
  {
    // List of encodings to try:
    static const std::vector<const char*> encodings{ "utf-16", "utf-8", "iso-8859-1" };

    std::string errorMessage;
    try {
      QXmlStreamReader reader(&file);
      return fn(reader);
    }
    catch (const XmlParseError& e) {
      MOBase::log::warn("The {} in this file is incorrectly encoded ({}). Applying heuristics...", file.fileName(), e.what());
    }

    // nmm's xml parser is less strict than the one from qt and allows files with
    // wrong encoding in the header. Being strict here would be bad user experience
    // this works around bad headers.
    QByteArray headerlessData = skipXmlHeader(file);

    // try parsing the file with several encodings to support broken files
    for (auto encoding : encodings) {
      MOBase::log::debug("Trying encoding {} for {}... ", encoding, file.fileName());
      try {
        QTextCodec* codec = QTextCodec::codecForName(encoding);
        QXmlStreamReader reader(codec->fromUnicode(QString("<?xml version=\"1.0\" encoding=\"%1\" ?>").arg(encoding)) + headerlessData);
        MOBase::log::debug("Interpreting {} as {}.", file.fileName(), encoding);
        return fn(reader);
      }
      catch (const XmlParseError& e) {
        MOBase::log::debug("Not {}: {}.", encoding, e.what());
      }
    }
    
    throw XmlParseError(tr("Failed to parse %1. See console for details.").arg(file.fileName()));
  }

  static std::tuple<QString, int, QString> parseInfo(QXmlStreamReader& reader)
  {
    std::tuple<QString, int, QString> info{ "", -1, "" };
    while (!reader.atEnd()) {
      switch (reader.readNext()) {
      case QXmlStreamReader::StartElement: {
        if (reader.name() == "Name") {
          std::get<0>(info) = reader.readElementText();
        }
        else if (reader.name() == "Author") {
        }
        else if (reader.name() == "Version") {
          std::get<2>(info) = reader.readElementText();
        }
        else if (reader.name() == "Id") {
          std::get<1>(info) = reader.readElementText().toInt();
        }
        else if (reader.name() == "Website") {
        }
      } break;
      default: {} break;
      }
    }
    if (reader.hasError()) {
      throw XmlParseError(QString("%1 in line %2").arg(reader.errorString()).arg(reader.lineNumber()));
    }
    return info;
  }

};

#endif