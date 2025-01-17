// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_util.h"

#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/zoom/page_zoom_constants.h"
#include "pdf/pdf_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace pdf_extension_util {

namespace {

// Tags in the manifest to be replaced.
const char kNameTag[] = "<NAME>";

// Adds strings that are used both by the stand-alone PDF Viewer and the Print
// Preview PDF Viewer.
void AddCommonStrings(base::Value* dict) {
  static constexpr webui::LocalizedString kPdfResources[] = {
      {"errorDialogTitle", IDS_PDF_ERROR_DIALOG_TITLE},
      {"pageLoadFailed", IDS_PDF_PAGE_LOAD_FAILED},
      {"pageLoading", IDS_PDF_PAGE_LOADING},
      {"pageReload", IDS_PDF_PAGE_RELOAD_BUTTON},
      {"tooltipFitToPage", IDS_PDF_TOOLTIP_FIT_PAGE},
      {"tooltipFitToWidth", IDS_PDF_TOOLTIP_FIT_WIDTH},
      {"tooltipZoomIn", IDS_PDF_TOOLTIP_ZOOM_IN},
      {"tooltipZoomOut", IDS_PDF_TOOLTIP_ZOOM_OUT},
      {"twoUpViewEnable", IDS_PDF_TWO_UP_VIEW_ENABLE},
  };
  for (const auto& resource : kPdfResources)
    dict->SetStringKey(resource.name, l10n_util::GetStringUTF16(resource.id));

  dict->SetStringKey("presetZoomFactors", zoom::GetPresetZoomFactorsAsJSON());
}

// Adds strings that are used only by the stand-alone PDF Viewer.
void AddPdfViewerStrings(base::Value* dict) {
  static constexpr webui::LocalizedString kPdfResources[] = {
    {"annotationsShowToggle", IDS_PDF_ANNOTATIONS_SHOW_TOGGLE},
    {"bookmarks", IDS_PDF_BOOKMARKS},
    {"bookmarkExpandIconAriaLabel", IDS_PDF_BOOKMARK_EXPAND_ICON_ARIA_LABEL},
    {"downloadEdited", IDS_PDF_DOWNLOAD_EDITED},
    {"downloadOriginal", IDS_PDF_DOWNLOAD_ORIGINAL},
    {"labelPageNumber", IDS_PDF_LABEL_PAGE_NUMBER},
    {"menu", IDS_MENU},
    {"moreActions", IDS_DOWNLOAD_MORE_ACTIONS},
    {"passwordDialogTitle", IDS_PDF_PASSWORD_DIALOG_TITLE},
    {"passwordInvalid", IDS_PDF_PASSWORD_INVALID},
    {"passwordPrompt", IDS_PDF_NEED_PASSWORD},
    {"passwordSubmit", IDS_PDF_PASSWORD_SUBMIT},
    {"present", IDS_PDF_PRESENT},
    {"propertiesApplication", IDS_PDF_PROPERTIES_APPLICATION},
    {"propertiesAuthor", IDS_PDF_PROPERTIES_AUTHOR},
    {"propertiesCreated", IDS_PDF_PROPERTIES_CREATED},
    {"propertiesDialogClose", IDS_CLOSE},
    {"propertiesDialogTitle", IDS_PDF_PROPERTIES_DIALOG_TITLE},
    {"propertiesFastWebView", IDS_PDF_PROPERTIES_FAST_WEB_VIEW},
    {"propertiesFastWebViewNo", IDS_PDF_PROPERTIES_FAST_WEB_VIEW_NO},
    {"propertiesFastWebViewYes", IDS_PDF_PROPERTIES_FAST_WEB_VIEW_YES},
    {"propertiesFileName", IDS_PDF_PROPERTIES_FILE_NAME},
    {"propertiesFileSize", IDS_PDF_PROPERTIES_FILE_SIZE},
    {"propertiesKeywords", IDS_PDF_PROPERTIES_KEYWORDS},
    {"propertiesModified", IDS_PDF_PROPERTIES_MODIFIED},
    {"propertiesPageCount", IDS_PDF_PROPERTIES_PAGE_COUNT},
    {"propertiesPageSize", IDS_PDF_PROPERTIES_PAGE_SIZE},
    {"propertiesPdfProducer", IDS_PDF_PROPERTIES_PDF_PRODUCER},
    {"propertiesPdfVersion", IDS_PDF_PROPERTIES_PDF_VERSION},
    {"propertiesSubject", IDS_PDF_PROPERTIES_SUBJECT},
    {"propertiesTitle", IDS_PDF_PROPERTIES_TITLE},
    {"thumbnailPageAriaLabel", IDS_PDF_THUMBNAIL_PAGE_ARIA_LABEL},
    {"tooltipDocumentOutline", IDS_PDF_TOOLTIP_DOCUMENT_OUTLINE},
    {"tooltipDownload", IDS_PDF_TOOLTIP_DOWNLOAD},
    {"tooltipPrint", IDS_PDF_TOOLTIP_PRINT},
    {"tooltipRotateCCW", IDS_PDF_TOOLTIP_ROTATE_CCW},
    {"tooltipRotateCW", IDS_PDF_TOOLTIP_ROTATE_CW},
    {"tooltipThumbnails", IDS_PDF_TOOLTIP_THUMBNAILS},
    {"zoomTextInputAriaLabel", IDS_PDF_ZOOM_TEXT_INPUT_ARIA_LABEL},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"tooltipAnnotate", IDS_PDF_ANNOTATION_ANNOTATE},
    {"annotationDocumentTooLarge", IDS_PDF_ANNOTATION_DOCUMENT_TOO_LARGE},
    {"annotationDocumentProtected", IDS_PDF_ANNOTATION_DOCUMENT_PROTECTED},
    {"annotationDocumentRotated", IDS_PDF_ANNOTATION_DOCUMENT_ROTATED},
    {"annotationEditInDefaultView", IDS_PDF_ANNOTATION_EDIT_IN_DEFAULT_VIEW},
    {"annotationResetRotate", IDS_PDF_ANNOTATION_RESET_ROTATE},
    {"annotationResetTwoPageView", IDS_PDF_ANNOTATION_RESET_TWO_PAGE_VIEW},
    {"annotationResetRotateAndTwoPageView",
     IDS_PDF_ANNOTATION_RESET_ROTATE_AND_TWO_PAGE_VIEW},
    {"cancelButton", IDS_CANCEL},
    {"annotationPen", IDS_PDF_ANNOTATION_PEN},
    {"annotationHighlighter", IDS_PDF_ANNOTATION_HIGHLIGHTER},
    {"annotationEraser", IDS_PDF_ANNOTATION_ERASER},
    {"annotationUndo", IDS_PDF_ANNOTATION_UNDO},
    {"annotationRedo", IDS_PDF_ANNOTATION_REDO},
    {"annotationExpand", IDS_PDF_ANNOTATION_EXPAND},
    {"annotationColorBlack", IDS_PDF_ANNOTATION_COLOR_BLACK},
    {"annotationColorRed", IDS_PDF_ANNOTATION_COLOR_RED},
    {"annotationColorYellow", IDS_PDF_ANNOTATION_COLOR_YELLOW},
    {"annotationColorGreen", IDS_PDF_ANNOTATION_COLOR_GREEN},
    {"annotationColorCyan", IDS_PDF_ANNOTATION_COLOR_CYAN},
    {"annotationColorPurple", IDS_PDF_ANNOTATION_COLOR_PURPLE},
    {"annotationColorBrown", IDS_PDF_ANNOTATION_COLOR_BROWN},
    {"annotationColorWhite", IDS_PDF_ANNOTATION_COLOR_WHITE},
    {"annotationColorCrimson", IDS_PDF_ANNOTATION_COLOR_CRIMSON},
    {"annotationColorAmber", IDS_PDF_ANNOTATION_COLOR_AMBER},
    {"annotationColorAvocadoGreen", IDS_PDF_ANNOTATION_COLOR_AVOCADO_GREEN},
    {"annotationColorCobaltBlue", IDS_PDF_ANNOTATION_COLOR_COBALT_BLUE},
    {"annotationColorDeepPurple", IDS_PDF_ANNOTATION_COLOR_DEEP_PURPLE},
    {"annotationColorDarkBrown", IDS_PDF_ANNOTATION_COLOR_DARK_BROWN},
    {"annotationColorDarkGrey", IDS_PDF_ANNOTATION_COLOR_DARK_GREY},
    {"annotationColorHotPink", IDS_PDF_ANNOTATION_COLOR_HOT_PINK},
    {"annotationColorOrange", IDS_PDF_ANNOTATION_COLOR_ORANGE},
    {"annotationColorLime", IDS_PDF_ANNOTATION_COLOR_LIME},
    {"annotationColorBlue", IDS_PDF_ANNOTATION_COLOR_BLUE},
    {"annotationColorViolet", IDS_PDF_ANNOTATION_COLOR_VIOLET},
    {"annotationColorTeal", IDS_PDF_ANNOTATION_COLOR_TEAL},
    {"annotationColorLightGrey", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREY},
    {"annotationColorLightPink", IDS_PDF_ANNOTATION_COLOR_LIGHT_PINK},
    {"annotationColorLightOrange", IDS_PDF_ANNOTATION_COLOR_LIGHT_ORANGE},
    {"annotationColorLightGreen", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREEN},
    {"annotationColorLightBlue", IDS_PDF_ANNOTATION_COLOR_LIGHT_BLUE},
    {"annotationColorLavender", IDS_PDF_ANNOTATION_COLOR_LAVENDER},
    {"annotationColorLightTeal", IDS_PDF_ANNOTATION_COLOR_LIGHT_TEAL},
    {"annotationSize1", IDS_PDF_ANNOTATION_SIZE1},
    {"annotationSize2", IDS_PDF_ANNOTATION_SIZE2},
    {"annotationSize3", IDS_PDF_ANNOTATION_SIZE3},
    {"annotationSize4", IDS_PDF_ANNOTATION_SIZE4},
    {"annotationSize8", IDS_PDF_ANNOTATION_SIZE8},
    {"annotationSize12", IDS_PDF_ANNOTATION_SIZE12},
    {"annotationSize16", IDS_PDF_ANNOTATION_SIZE16},
    {"annotationSize20", IDS_PDF_ANNOTATION_SIZE20},
    {"annotationFormWarningTitle", IDS_PDF_DISCARD_FORM_CHANGES},
    {"annotationFormWarningDetail", IDS_PDF_DISCARD_FORM_CHANGES_DETAIL},
    {"annotationFormWarningKeepEditing", IDS_PDF_KEEP_EDITING},
    {"annotationFormWarningDiscard", IDS_PDF_DISCARD},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  };
  for (const auto& resource : kPdfResources)
    dict->SetStringKey(resource.name, l10n_util::GetStringUTF16(resource.id));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::string16 edit_string = l10n_util::GetStringUTF16(IDS_EDIT);
  base::Erase(edit_string, '&');
  dict->SetStringKey("editButton", edit_string);
#endif

  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 static_cast<base::DictionaryValue*>(dict));
}

}  // namespace

std::string GetManifest() {
  std::string manifest_contents = ui::ResourceBundle::GetSharedInstance()
                                      .GetRawDataResource(IDR_PDF_MANIFEST)
                                      .as_string();
  DCHECK(manifest_contents.find(kNameTag) != std::string::npos);
  base::ReplaceFirstSubstringAfterOffset(
      &manifest_contents, 0, kNameTag,
      ChromeContentClient::kPDFExtensionPluginName);

  return manifest_contents;
}

void AddStrings(PdfViewerContext context, base::Value* dict) {
  AddCommonStrings(dict);
  if (context == PdfViewerContext::kPdfViewer ||
      context == PdfViewerContext::kAll) {
    AddPdfViewerStrings(dict);
  }
  if (context == PdfViewerContext::kPrintPreview ||
      context == PdfViewerContext::kAll) {
    // Nothing to do yet, since there are no PrintPreview-only strings.
  }
}

void AddAdditionalData(base::Value* dict) {
  dict->SetKey("documentPropertiesEnabled",
               base::Value(base::FeatureList::IsEnabled(
                   chrome_pdf::features::kPdfViewerDocumentProperties)));
  dict->SetKey("presentationModeEnabled",
               base::Value(base::FeatureList::IsEnabled(
                   chrome_pdf::features::kPdfViewerPresentationMode)));

  bool enable_printing = true;
  bool enable_annotations = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For Chrome OS, enable printing only if we are not at OOBE.
  enable_printing = !chromeos::LoginDisplayHost::default_host();
  enable_annotations = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  dict->SetKey("printingEnabled", base::Value(enable_printing));
  dict->SetKey("pdfAnnotationsEnabled", base::Value(enable_annotations));
}

}  // namespace pdf_extension_util
