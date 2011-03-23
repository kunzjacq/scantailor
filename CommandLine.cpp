#include <cstdlib>
#include <assert.h>
#include <iostream>

#include <QMap>
#include <QRegExp>
#include <QStringList>

#include "Dpi.h"
#include "ImageId.h"
#include "version.h"
#include "CommandLine.h"
#include "ImageFileInfo.h"
#include "ImageMetadata.h"
#include "filters/page_split/LayoutType.h"
#include "filters/page_layout/Settings.h"
#include "Margins.h"
#include "Despeckle.h"


CommandLine CommandLine::m_globalInstance;


void
CommandLine::set(CommandLine const& cl)
{
	assert(!m_globalInstance.isGlobal());

	m_globalInstance = cl;
	m_globalInstance.setGlobal();
}


void
CommandLine::parseCli(QStringList const& argv)
{
	QRegExp rx("^--([^=]+)=(.*)$");
	QRegExp rx_switch("^--([^=]+)$");
	QRegExp rx_short("^-([^=]+)=(.*)$");
	QRegExp rx_short_switch("^-([^=]+)$");
	QRegExp rx_project(".*\\.ScanTailor$", Qt::CaseInsensitive);

	QMap<QString, QString> shortMap;
	shortMap["h"] = "help";
	shortMap["help"] = "help";
	shortMap["v"] = "verbose";
	shortMap["l"] = "layout";
	shortMap["ld"] = "layout-direction";
	shortMap["o"] = "output-project";

	// skip first argument (scantailor)
	for (int i=1; i<argv.size(); i++) {
#ifdef DEBUG_CLI
	std::cout << "arg[" << i << "]=" << argv[i].toAscii().constData() << "\n";
#endif
		if (rx.exactMatch(argv[i])) {
			// option with a value
			m_options[rx.cap(1)] = rx.cap(2);
		} else if (rx_switch.exactMatch(argv[i])) {
			// option without value
			m_options[rx_switch.cap(1)] = "true";
		} else if (rx_short.exactMatch(argv[i])) {
			// option with a value
			QString key = shortMap[rx_short.cap(1)];
			m_options[key] = rx_short.cap(2);
		} else if (rx_short_switch.exactMatch(argv[i])) {
			QString key = shortMap[rx_short_switch.cap(1)];
			if (key == "") continue;
			m_options[key] = "true";
		} else if (rx_project.exactMatch(argv[i])) {
			// project file
			CommandLine::m_projectFile = argv[i];
		} else {
			// image and output directory
			QFileInfo file(argv[i]);
			CommandLine::m_files.push_back(file);
			if (i==(argv.size()-1)) {
				// output directory
				if (file.isDir()) {
					CommandLine::m_outputDirectory = file.filePath();
				} else {
					std::cout << "Error: Last argument must be an existing directory" << "\n";
					exit(1);
				}
			} else {
				// create ImageFileInfo and push to images
				ImageId const image_id(file.filePath());
				ImageMetadata metadata;
				metadata.setDpi(CommandLine::fetchDpi());
				std::vector<ImageMetadata> vMetadata;
				vMetadata.push_back(metadata);
				ImageFileInfo image_info(file, vMetadata);
				CommandLine::m_images.push_back(image_info);
			}
		}
	}

	setup();

#ifdef DEBUG_CLI
	QStringList params = m_options.keys();
	for (int i=0; i<params.size(); i++) { std::cout << params[i].toAscii().constData() << "=" << m_options[params[i]].toAscii().constData() << "\n"; }
	std::cout << "Images: " << CommandLine::m_images.size() << "\n";
#endif
}


void
CommandLine::setup()
{
	m_outputProjectFile = fetchOutputProjectFile();
	m_layoutType = fetchLayoutType();
	m_layoutDirection = fetchLayoutDirection();
	m_colorMode = fetchColorMode();
	m_dpi = fetchDpi();
	m_outputDpi = fetchDpi("output-dpi");
	m_margins = fetchMargins();
	m_alignment = fetchAlignment();
	m_contentDetection = fetchContentDetection();
	m_contentRect = fetchContentRect();
	m_orientation = fetchOrientation();
	m_threshold = fetchThreshold();
	m_deskewAngle = fetchDeskewAngle();
	m_startFilterIdx = fetchStartFilterIdx();
	m_endFilterIdx = fetchEndFilterIdx();
}


void
CommandLine::printHelp()
{
	std::cout << "\n";
	std::cout << "Scan Tailor is a post-processing tool for scanned pages." << "\n";
	std::cout << "Version: " << VERSION << "\n";
	std::cout << "\n";
	std::cout << "ScanTailor usage: " << "\n";
	std::cout << "\t1) scantailor" << "\n";
	std::cout << "\t2) scantailor <project_file>" << "\n";
	std::cout << "\t3) scantailor-batch [options] <image, image, ...> <output_directory>" << "\n";
	std::cout << "\t4) scantailor-batch [options] <project_file> [output_directory]" << "\n";
	std::cout << "\n";
	std::cout << "1)" << "\n";
	std::cout << "\tstart ScanTailor's GUI interface" << "\n";
	std::cout << "2)" << "\n";
	std::cout << "\tstart ScanTailor's GUI interface and load project file" << "\n";
	std::cout << "3)" << "\n";
	std::cout << "\tbatch processing images from command line; no GUI" << "\n";
	std::cout << "4)" << "\n";
	std::cout << "\tbatch processing project from command line; no GUI" << "\n";
	std::cout << "\tif output_directory is specified as last argument, it overwrites the one in project file" << "\n";
	std::cout << "\n";
	std::cout << "Options:" << "\n";
	std::cout << "\t--help, -h" << "\n";
	std::cout << "\t--verbose, -v" << "\n";
	std::cout << "\t--layout=, -l=<0|1|1.5|2>\t\t-- default: 0" << "\n";
	std::cout << "\t--layout-direction=, -ld=<lr|rl>\t-- default: lr" << "\n";
	std::cout << "\t--orientation=<left|right|upsidedown|none>\n\t\t\t\t\t\t-- default: none" << "\n";
	std::cout << "\t--rotate=<0.0...360.0>\t\t\t-- it also sets deskew to manual mode" << "\n";
	std::cout << "\t--deskew=<auto|manual>\t\t\t-- default: auto" << "\n";
	std::cout << "\t--content-detection=<cautious|normal|aggressive>\n\t\t\t\t\t\t-- default: normal" << "\n";
	std::cout << "\t--content-box=<<left_offset>x<top_offset>:<width>x<height>>" << "\n";
	std::cout << "\t\t\t\t\t\t-- if set the content detection is se to manual mode" << "\n";
	std::cout << "\t\t\t\t\t\t   example: --content-box=100x100:1500x2500" << "\n";
	std::cout << "\t--margins=<number>\t\t\t-- sets left, top, right and bottom margins to same number." << "\n";
	std::cout << "\t\t--margins-left=<number>" << "\n";
	std::cout << "\t\t--margins-right=<number>" << "\n";
	std::cout << "\t\t--margins-top=<number>" << "\n";
	std::cout << "\t\t--margins-bottom=<number>" << "\n";
	std::cout << "\t--match-layout=<true|false>\t\t-- default: true" << "\n";
	std::cout << "\t--match-layout-tolerance=<0.0...)\t-- default: off" << "\n";
	std::cout << "\t--alignment=center\t\t-- sets vertical and horizontal alignment to center" << "\n";
	std::cout << "\t\t--alignment-vertical=<top|center|bottom>" << "\n";
	std::cout << "\t\t--alignment-horizontal=<left|center|right>" << "\n";
	std::cout << "\t--dpi=<number>\t\t\t\t-- sets x and y dpi. default: 600" << "\n";
	std::cout << "\t\t--dpi-x=<number>" << "\n";
	std::cout << "\t\t--dpi-y=<number>" << "\n";
	std::cout << "\t--output-dpi=<number>\t\t\t-- sets x and y output dpi. default: 600" << "\n";
	std::cout << "\t\t--output-dpi-x=<number>" << "\n";
	std::cout << "\t\t--output-dpi-y=<number>" << "\n";
	std::cout << "\t--color-mode=<black_and_white|color_grayscale|mixed>\n\t\t\t\t\t\t-- default: black_and_white" << "\n";
	std::cout << "\t--white-margins\t\t\t\t-- default: false" << "\n";
	std::cout << "\t--normalize-illumination\t\t-- default: false" << "\n";
	std::cout << "\t--threshold=<n>\t\t\t\t-- n<0 thinner, n>0 thicker; default: 0" << "\n";
	std::cout << "\t--despeckle=<off|cautious|normal|aggressive>\n\t\t\t\t\t\t-- default: normal" << "\n";
	std::cout << "\t--dewarping=<off|auto>\t\t\t-- default: off" << "\n";
	std::cout << "\t--depth-perception=<1.0...3.0>\t\t-- default: 2.0" << "\n";
	std::cout << "\t--start-filter=<1...6>\t\t\t-- default: 4" << "\n";
	std::cout << "\t--end-filter=<1...6>\t\t\t-- default: 6" << "\n";
	std::cout << "\t--output-project=, -o=<project_name>" << "\n";
	std::cout << "\n";
}


page_split::LayoutType
CommandLine::fetchLayoutType()
{
	page_split::LayoutType lt = page_split::AUTO_LAYOUT_TYPE;

	if (!m_options.contains("layout"))
		return lt;

	if (m_options["layout"] == "1")
		lt = page_split::SINGLE_PAGE_UNCUT;
	else if (m_options["layout"] == "1.5")
		lt = page_split::PAGE_PLUS_OFFCUT;
	else if (m_options["layout"] == "2")
		lt = page_split::TWO_PAGES;

	return lt;
}

Qt::LayoutDirection
CommandLine::fetchLayoutDirection()
{
	Qt::LayoutDirection l = Qt::LeftToRight;
	if (m_options.contains("layout-direction"))
		return l;

	QString ld = m_options["layout-direction"].toLower();
	if (ld == "rl")
		l = Qt::RightToLeft;

	return l;
}

Dpi
CommandLine::fetchDpi(QString oname)
{
	int xdpi=600;
	int ydpi=600;

	if (m_options.contains(oname+"-x")) {
		xdpi = m_options[oname+"-x"].toInt();
	}
	if (m_options.contains(oname+"-y")) {
		ydpi = m_options[oname+"-y"].toInt();
	}
	if (m_options.contains(oname)) {
		xdpi = m_options[oname].toInt();
		ydpi = m_options[oname].toInt();
	}

	return Dpi(xdpi, ydpi);
}

output::ColorParams::ColorMode
CommandLine::fetchColorMode()
{
	QString cm = m_options["color-mode"].toLower();
	
	if (cm == "color_grayscale")
		return output::ColorParams::COLOR_GRAYSCALE;
	else if (cm == "mixed")
		return output::ColorParams::MIXED;

	return output::ColorParams::BLACK_AND_WHITE;
}


Margins
CommandLine::fetchMargins()
{
	Margins margins(page_layout::Settings::defaultHardMarginsMM());

	if (m_options.contains("margins")) {
		double m = m_options["margins"].toDouble();
		margins.setTop(m);
		margins.setBottom(m);
		margins.setLeft(m);
		margins.setRight(m);
	}

	if (m_options.contains("margins-left"))
		margins.setLeft(m_options["margins-left"].toFloat());
	if (m_options.contains("margins-right"))
		margins.setRight(m_options["margins-right"].toFloat());
	if (m_options.contains("margins-top"))
		margins.setTop(m_options["margins-top"].toFloat());
	if (m_options.contains("margins-bottom"))
		margins.setBottom(m_options["margins-bottom"].toFloat());

	return margins;
}

page_layout::Alignment
CommandLine::fetchAlignment()
{
	page_layout::Alignment alignment(page_layout::Alignment::TOP, page_layout::Alignment::HCENTER);

	if (m_options.contains("match-layout")) {
		if (m_options["match-layout"] == "false") alignment.setNull(true);
		if (m_options["match-layout"] == "true") alignment.setNull(false);
	}

	if (m_options.contains("alignment")) {
		alignment.setVertical(page_layout::Alignment::VCENTER);
		alignment.setHorizontal(page_layout::Alignment::HCENTER);
	}

	if (m_options.contains("alignment-vertical")) {
		QString a = m_options["alignment-vertical"].toLower();
		if (a == "top") alignment.setVertical(page_layout::Alignment::TOP);
		if (a == "center") alignment.setVertical(page_layout::Alignment::VCENTER);
		if (a == "bottom") alignment.setVertical(page_layout::Alignment::BOTTOM);
	}

	if (m_options.contains("alignment-horizontal")) {
		QString a = m_options["alignment-horizontal"].toLower();
		if (a == "left") alignment.setHorizontal(page_layout::Alignment::LEFT);
		if (a == "center") alignment.setHorizontal(page_layout::Alignment::HCENTER);
		if (a == "right") alignment.setHorizontal(page_layout::Alignment::RIGHT);
	}

	return alignment;
}

Despeckle::Level
CommandLine::fetchContentDetection()
{
	Despeckle::Level level = Despeckle::NORMAL;

	if (m_options["content-detection"] != "") {
		QString cm = m_options["content-detection"].toLower();
		if (cm == "cautious")
			level = Despeckle::CAUTIOUS;
		else if (cm == "aggressive")
			level = Despeckle::AGGRESSIVE;
	}

	return level;
}


QRectF
CommandLine::fetchContentRect()
{
	if (!m_options.contains("content-box"))
		return QRectF();

	QRegExp rx("([\\d\\.]+)x([\\d\\.]+):([\\d\\.]+)x([\\d\\.]+)");

	if (rx.exactMatch(m_options["content-box"])) {
		return QRectF(rx.cap(1).toFloat(), rx.cap(2).toFloat(), rx.cap(3).toFloat(), rx.cap(4).toFloat());
	}

	std::cout << "invalid --content-box=" << m_options["content-box"].toAscii().constData() << "\n";
	exit(1);
}


CommandLine::Orientation
CommandLine::fetchOrientation()
{
	if (!m_options.contains("orientation"))
		return TOP;

	Orientation orient;
	QString cli_orient = m_options["orientation"];

	if (cli_orient == "left") {
		orient = LEFT;
	} else if (cli_orient == "right") {
		orient = RIGHT;
	} else if (cli_orient == "upsidedown") {
		orient = UPSIDEDOWN;
	} else {
		std::cout << "Wrong orientation " << m_options["orientation"].toAscii().constData() << "\n";
		exit(1);
	}

	return orient;
}


QString
CommandLine::fetchOutputProjectFile()
{
	if (!m_options.contains("output-project"))
		return QString();

	return m_options["output-project"];
}

int
CommandLine::fetchThreshold()
{
	if (!m_options.contains("threshold"))
		return 0;

	return m_options["threshold"].toInt();
}

double
CommandLine::fetchDeskewAngle()
{
	if (!m_options.contains("rotate"))
		return 0.0;

	return m_options["rotate"].toDouble();
}

int
CommandLine::fetchStartFilterIdx()
{
	if (!m_options.contains("start-filter"))
		return 0;

	return m_options["start-filter"].toInt() - 1;
}

int
CommandLine::fetchEndFilterIdx()
{
	if (!m_options.contains("start-filter"))
		return 5;

	return m_options["end-filter"].toInt() - 1;
}

output::DewarpingMode
CommandLine::fetchDewarpingMode()
{
	if (!m_options.contains("dewarping"))
		return output::DewarpingMode::OFF;

	return output::DewarpingMode(m_options["dewarping"].toLower());
}

output::DespeckleLevel
CommandLine::fetchDespeckleLevel()
{
	if (!m_options.contains("despeckle"))
		return output::DESPECKLE_NORMAL;

	return output::despeckleLevelFromString(m_options["despeckle"]);
}

output::DepthPerception
CommandLine::fetchDepthPerception()
{
	if (!m_options.contains("depth-perception"))
		return output::DepthPerception();

	return output::DepthPerception(m_options["depth-perception"]);
}

float
CommandLine::fetchMatchLayoutTolerance()
{
	if (!m_options.contains("match-layout-tolerance"))
		return 0.2;

	return m_options["match-layout-tolerance"].toFloat();
}


bool
CommandLine::containsMargins()
{
	return(
		m_options.contains("margins") ||
		m_options.contains("margins-left") ||
		m_options.contains("margins-right") ||
		m_options.contains("margins-top") ||
		m_options.contains("margins-bottom")
	);
}

bool
CommandLine::containsAlignment()
{
	return(
		m_options.contains("match-layout-tolerance") ||
		m_options.contains("alignment") ||
		m_options.contains("alignment-vertical") ||
		m_options.contains("alignment-horizontal")
	);
}

bool
CommandLine::containsOutputDpi()
{
	return(
		m_options.contains("output-dpi") ||
		m_options.contains("output-dpi-x") ||
		m_options.contains("output-dpi-y")
	);
}
