#pragma once

#include <windows.h>
#include <cstring>
#include <vector>

#include "resource.h"

using namespace System;
using namespace System::Drawing;
using namespace System::Drawing::Imaging;
using namespace System::IO;
using namespace Microsoft::Win32;
using namespace System::Windows::Forms;

ref class ClipBirdMenuColorTable : public ProfessionalColorTable
{
private:
    bool dark;

public:
    ClipBirdMenuColorTable(bool useDarkTheme)
    {
        dark = useDarkTheme;
    }

    virtual property Color ToolStripDropDownBackground
    {
        Color get() override
        {
            return dark ? Color::FromArgb(43, 43, 43) : Color::FromArgb(249, 249, 249);
        }
    }

    virtual property Color MenuBorder
    {
        Color get() override
        {
            return dark ? Color::FromArgb(69, 69, 69) : Color::FromArgb(229, 229, 229);
        }
    }

    virtual property Color MenuItemBorder
    {
        Color get() override
        {
            return MenuBorder;
        }
    }

    virtual property Color MenuItemSelected
    {
        Color get() override
        {
            return dark ? Color::FromArgb(58, 58, 58) : Color::FromArgb(240, 240, 240);
        }
    }

    virtual property Color MenuItemSelectedGradientBegin
    {
        Color get() override
        {
            return MenuItemSelected;
        }
    }

    virtual property Color MenuItemSelectedGradientEnd
    {
        Color get() override
        {
            return MenuItemSelected;
        }
    }

    virtual property Color MenuItemPressedGradientBegin
    {
        Color get() override
        {
            return MenuItemSelected;
        }
    }

    virtual property Color MenuItemPressedGradientMiddle
    {
        Color get() override
        {
            return MenuItemSelected;
        }
    }

    virtual property Color MenuItemPressedGradientEnd
    {
        Color get() override
        {
            return MenuItemSelected;
        }
    }

    virtual property Color ImageMarginGradientBegin
    {
        Color get() override
        {
            return ToolStripDropDownBackground;
        }
    }

    virtual property Color ImageMarginGradientMiddle
    {
        Color get() override
        {
            return ToolStripDropDownBackground;
        }
    }

    virtual property Color ImageMarginGradientEnd
    {
        Color get() override
        {
            return ToolStripDropDownBackground;
        }
    }

    virtual property Color SeparatorDark
    {
        Color get() override
        {
            return MenuBorder;
        }
    }

    virtual property Color SeparatorLight
    {
        Color get() override
        {
            return MenuBorder;
        }
    }
};

public ref class Form1 : public Form
{
private:
    HWND hwnd;
    bool clipboardListenerRegistered;
    DWORD lastOwnedClipboardSequenceNumber;
    System::Collections::Generic::List<String^>^ temporaryScreenshotPaths;
    NotifyIcon^ trayIcon;
    System::Drawing::Icon^ trayIconImage;
    System::Windows::Forms::ContextMenuStrip^ trayMenu;
    ToolStripMenuItem^ convertClipboardTextItem;
    ToolStripMenuItem^ allowScreenshotFileDropItem;
    ToolStripMenuItem^ removeUtmParametersItem;
    bool trayMenuThemeInitialized;
    bool trayMenuUsesDarkTheme;

public:
    Form1()
    {
        hwnd = nullptr;
        clipboardListenerRegistered = false;
        lastOwnedClipboardSequenceNumber = 0;
        temporaryScreenshotPaths =
            gcnew System::Collections::Generic::List<String^>();
        trayIcon = nullptr;
        trayIconImage = nullptr;
        trayMenu = nullptr;
        convertClipboardTextItem = nullptr;
        allowScreenshotFileDropItem = nullptr;
        removeUtmParametersItem = nullptr;
        trayMenuThemeInitialized = false;
        trayMenuUsesDarkTheme = false;
        InitializeComponent();
        hwnd = static_cast<HWND>(this->Handle.ToPointer());
    }

protected:
    virtual void SetVisibleCore(bool) override
    {
        Form::SetVisibleCore(false);
    }

    virtual void OnHandleCreated(EventArgs^ e) override
    {
        Form::OnHandleCreated(e);

        hwnd = static_cast<HWND>(this->Handle.ToPointer());
        clipboardListenerRegistered = AddClipboardFormatListener(hwnd) != FALSE;
        if (!clipboardListenerRegistered)
        {
            DWORD errorCode = GetLastError();
            ShowTrayError(String::Format(
                "Clipboard monitoring could not start (Windows error {0}).",
                errorCode));
        }
    }

    virtual void OnHandleDestroyed(EventArgs^ e) override
    {
        if (clipboardListenerRegistered)
        {
            RemoveClipboardFormatListener(hwnd);
            clipboardListenerRegistered = false;
        }

        Form::OnHandleDestroyed(e);
    }

    virtual void OnFormClosed(FormClosedEventArgs^ e) override
    {
        if (trayIcon != nullptr)
        {
            trayIcon->Visible = false;
            delete trayIcon;
            trayIcon = nullptr;
        }

        if (trayIconImage != nullptr)
        {
            delete trayIconImage;
            trayIconImage = nullptr;
        }

        if (trayMenu != nullptr)
        {
            delete trayMenu;
            trayMenu = nullptr;
        }

        Form::OnFormClosed(e);
    }

private:
    void InitializeComponent()
    {
        this->ShowInTaskbar = false;

        trayMenu = gcnew System::Windows::Forms::ContextMenuStrip();
        trayMenu->Opening +=
            gcnew System::ComponentModel::CancelEventHandler(
                this, &Form1::OnTrayMenuOpening);

        convertClipboardTextItem =
            gcnew ToolStripMenuItem("Convert clipboard text to plaintext.");
        convertClipboardTextItem->CheckOnClick = true;
        convertClipboardTextItem->Checked = true;
        trayMenu->Items->Add(convertClipboardTextItem);

        allowScreenshotFileDropItem =
            gcnew ToolStripMenuItem("Let screenshots be pastable to explorer.");
        allowScreenshotFileDropItem->CheckOnClick = true;
        allowScreenshotFileDropItem->Checked = true;
        trayMenu->Items->Add(allowScreenshotFileDropItem);

        removeUtmParametersItem =
            gcnew ToolStripMenuItem("Remove UTM parameters from single URLs.");
        removeUtmParametersItem->CheckOnClick = true;
        removeUtmParametersItem->Checked = true;
        trayMenu->Items->Add(removeUtmParametersItem);

        trayMenu->Items->Add(gcnew ToolStripSeparator());

        ToolStripMenuItem^ exitItem = gcnew ToolStripMenuItem("Exit");
        exitItem->Click += gcnew EventHandler(this, &Form1::ExitFromTray);
        trayMenu->Items->Add(exitItem);

        trayIconImage = CreateParrotIcon();
        trayIcon = gcnew NotifyIcon();
        trayIcon->Icon = trayIconImage;
        trayIcon->Text = "ClipBird - Clipboard Monitor";
        trayIcon->ContextMenuStrip = trayMenu;
        trayIcon->Visible = true;
        UpdateTrayMenuTheme();
    }

protected:
    virtual void WndProc(Message% m) override
    {
        if (m.Msg == WM_CLIPBOARDUPDATE)
        {
            ProcessClipboardUpdate();
            return;
        }

        Form::WndProc(m);
    }

private:
    System::Drawing::Icon^ CreateParrotIcon()
    {
        HRSRC imageResource = FindResource(
            nullptr, MAKEINTRESOURCE(IDR_PARROT), RT_RCDATA);
        if (imageResource == nullptr)
        {
            DWORD errorCode = GetLastError();
            throw gcnew InvalidOperationException(String::Format(
                "Windows could not find the embedded parrot image (error {0}).",
                errorCode));
        }

        DWORD resourceSize = SizeofResource(nullptr, imageResource);
        HGLOBAL resourceDataHandle = LoadResource(nullptr, imageResource);
        if (resourceSize == 0 || resourceDataHandle == nullptr)
        {
            DWORD errorCode = GetLastError();
            throw gcnew InvalidOperationException(String::Format(
                "Windows could not load the embedded parrot image (error {0}).",
                errorCode));
        }

        const void* resourceData = LockResource(resourceDataHandle);
        if (resourceData == nullptr || resourceSize > Int32::MaxValue)
        {
            throw gcnew InvalidOperationException(
                "The embedded parrot image is invalid or too large.");
        }

        array<Byte>^ imageBytes = gcnew array<Byte>(
            static_cast<int>(resourceSize));
        System::Runtime::InteropServices::Marshal::Copy(
            IntPtr(const_cast<void*>(resourceData)),
            imageBytes,
            0,
            imageBytes->Length);
        MemoryStream^ imageStream = gcnew MemoryStream(imageBytes);
        Image^ sourceImage = nullptr;
        try
        {
            sourceImage = Image::FromStream(imageStream);
            Bitmap^ bitmap = gcnew Bitmap(32, 32);
            try
            {
                Graphics^ graphics = Graphics::FromImage(bitmap);
                try
                {
                    graphics->Clear(Color::Transparent);
                    graphics->InterpolationMode =
                        System::Drawing::Drawing2D::InterpolationMode::HighQualityBicubic;
                    graphics->PixelOffsetMode =
                        System::Drawing::Drawing2D::PixelOffsetMode::HighQuality;

                    float scale = Math::Min(
                        32.0f / sourceImage->Width,
                        32.0f / sourceImage->Height);
                    float width = sourceImage->Width * scale;
                    float height = sourceImage->Height * scale;
                    RectangleF bounds(
                        (32.0f - width) / 2.0f,
                        (32.0f - height) / 2.0f,
                        width,
                        height);
                    graphics->DrawImage(sourceImage, bounds);
                }
                finally
                {
                    delete graphics;
                }

                IntPtr iconHandle = bitmap->GetHicon();
                try
                {
                    System::Drawing::Icon^ sourceIcon =
                        System::Drawing::Icon::FromHandle(iconHandle);
                    try
                    {
                        return dynamic_cast<System::Drawing::Icon^>(
                            sourceIcon->Clone());
                    }
                    finally
                    {
                        delete sourceIcon;
                    }
                }
                finally
                {
                    DestroyIcon(static_cast<HICON>(iconHandle.ToPointer()));
                }
            }
            finally
            {
                delete bitmap;
            }
        }
        finally
        {
            if (sourceImage != nullptr)
            {
                delete sourceImage;
            }
            delete imageStream;
        }
    }

    void ExitFromTray(Object^ sender, EventArgs^ e)
    {
        this->Close();
    }

    void OnTrayMenuOpening(
        Object^ sender, System::ComponentModel::CancelEventArgs^ e)
    {
        UpdateTrayMenuTheme();
    }

    void UpdateTrayMenuTheme()
    {
        Object^ lightThemeSetting = nullptr;
        try
        {
            lightThemeSetting = Registry::GetValue(
                "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                "AppsUseLightTheme",
                1);
        }
        catch (System::IO::IOException^ ex)
        {
            ShowTrayError(String::Format(
                "Could not read the Windows app theme setting: {0}", ex->Message));
            return;
        }
        catch (System::Security::SecurityException^ ex)
        {
            ShowTrayError(String::Format(
                "Could not read the Windows app theme setting: {0}", ex->Message));
            return;
        }
        catch (UnauthorizedAccessException^ ex)
        {
            ShowTrayError(String::Format(
                "Could not read the Windows app theme setting: {0}", ex->Message));
            return;
        }

        bool useDarkTheme = Convert::ToInt32(lightThemeSetting) == 0;
        if (trayMenuThemeInitialized && trayMenuUsesDarkTheme == useDarkTheme)
        {
            return;
        }

        trayMenuUsesDarkTheme = useDarkTheme;
        trayMenuThemeInitialized = true;
        trayMenu->Renderer = gcnew ToolStripProfessionalRenderer(
            gcnew ClipBirdMenuColorTable(useDarkTheme));
        trayMenu->ForeColor = useDarkTheme
            ? Color::FromArgb(243, 243, 243)
            : Color::FromArgb(31, 31, 31);
    }

    void ProcessClipboardUpdate()
    {
        if (GetClipboardOwner() == hwnd ||
            GetClipboardSequenceNumber() == lastOwnedClipboardSequenceNumber)
        {
            return;
        }

        if (allowScreenshotFileDropItem->Checked && TryCopyClipboardImageAsFile())
        {
            return;
        }

        if (!convertClipboardTextItem->Checked &&
            !removeUtmParametersItem->Checked)
        {
            return;
        }

        bool clipboardOpened = false;
        for (int attempt = 0; attempt < 5; ++attempt)
        {
            if (OpenClipboard(hwnd))
            {
                clipboardOpened = true;
                break;
            }

            if (attempt < 4)
            {
                System::Threading::Thread::Sleep(20);
            }
        }

        if (!clipboardOpened)
        {
            DWORD errorCode = GetLastError();
            ShowTrayError(String::Format(
                "Clipboard is busy; text could not be read (Windows error {0}).",
                errorCode));
            return;
        }

        try
        {
            if (GetClipboardOwner() == hwnd)
            {
                return;
            }

            String^ clipboardText = nullptr;
            if (IsClipboardFormatAvailable(CF_UNICODETEXT))
            {
                clipboardText = ReadClipboardText(CF_UNICODETEXT);
            }
            else if (IsClipboardFormatAvailable(CF_TEXT))
            {
                clipboardText = ReadClipboardText(CF_TEXT);
            }

            if (clipboardText == nullptr)
            {
                trayIcon->Text = "ClipBird - no readable text";
                return;
            }

            String^ replacementText = clipboardText;
            bool utmParametersRemoved =
                removeUtmParametersItem->Checked &&
                TryRemoveUtmParameters(clipboardText, replacementText);
            if (!convertClipboardTextItem->Checked && !utmParametersRemoved)
            {
                return;
            }

            HGLOBAL replacementData = CreateUnicodeClipboardData(replacementText);
            try
            {
                if (!EmptyClipboard())
                {
                    DWORD errorCode = GetLastError();
                    throw gcnew InvalidOperationException(String::Format(
                        "Windows could not clear the clipboard (error {0}).",
                        errorCode));
                }

                if (SetClipboardData(CF_UNICODETEXT, replacementData) == nullptr)
                {
                    DWORD errorCode = GetLastError();
                    throw gcnew InvalidOperationException(String::Format(
                        "Windows could not set plain clipboard text (error {0}).",
                        errorCode));
                }

                replacementData = nullptr;
                lastOwnedClipboardSequenceNumber = GetClipboardSequenceNumber();
                CleanupTemporaryScreenshots(nullptr);
                trayIcon->Text = utmParametersRemoved
                    ? "ClipBird - UTM parameters removed"
                    : "ClipBird - clipboard converted to plain text";
            }
            finally
            {
                if (replacementData != nullptr)
                {
                    GlobalFree(replacementData);
                }
            }
        }
        catch (InvalidOperationException^ ex)
        {
            ShowTrayError(String::Format(
                "Could not process clipboard text: {0}", ex->Message));
        }
        finally
        {
            if (!CloseClipboard())
            {
                DWORD errorCode = GetLastError();
                ShowTrayError(String::Format(
                    "Windows could not close the clipboard (error {0}).",
                    errorCode));
            }
        }
    }

    bool TryRemoveUtmParameters(String^ text, String^% cleanedText)
    {
        cleanedText = text;
        String^ candidate = text->Trim();
        if (candidate->Length == 0 ||
            !Uri::IsWellFormedUriString(candidate, UriKind::Absolute))
        {
            return false;
        }

        Uri^ parsedUri = nullptr;
        if (!Uri::TryCreate(candidate, UriKind::Absolute, parsedUri) ||
            (!String::Equals(
                parsedUri->Scheme, Uri::UriSchemeHttp, StringComparison::OrdinalIgnoreCase) &&
             !String::Equals(
                parsedUri->Scheme, Uri::UriSchemeHttps, StringComparison::OrdinalIgnoreCase)))
        {
            return false;
        }

        int queryStart = candidate->IndexOf('?');
        int fragmentStart = candidate->IndexOf('#');
        if (queryStart < 0 ||
            (fragmentStart >= 0 && queryStart > fragmentStart))
        {
            return false;
        }

        int queryEnd = fragmentStart >= 0 ? fragmentStart : candidate->Length;
        String^ query = candidate->Substring(queryStart + 1, queryEnd - queryStart - 1);
        array<String^>^ parameters = query->Split('&');
        System::Collections::Generic::List<String^>^ retainedParameters =
            gcnew System::Collections::Generic::List<String^>();
        bool removedAny = false;

        for each (String^ parameter in parameters)
        {
            int valueSeparator = parameter->IndexOf('=');
            String^ name = valueSeparator < 0
                ? parameter
                : parameter->Substring(0, valueSeparator);
            String^ decodedName = Uri::UnescapeDataString(name);
            if (decodedName->StartsWith("utm_", StringComparison::OrdinalIgnoreCase))
            {
                removedAny = true;
            }
            else
            {
                retainedParameters->Add(parameter);
            }
        }

        if (!removedAny)
        {
            return false;
        }

        int leadingWhitespaceLength = text->Length - text->TrimStart()->Length;
        int trailingWhitespaceLength = text->Length - text->TrimEnd()->Length;
        String^ leadingWhitespace = text->Substring(0, leadingWhitespaceLength);
        String^ trailingWhitespace = text->Substring(
            text->Length - trailingWhitespaceLength, trailingWhitespaceLength);
        String^ queryPrefix = retainedParameters->Count > 0
            ? "?" + String::Join("&", retainedParameters->ToArray())
            : String::Empty;
        String^ fragment = fragmentStart >= 0
            ? candidate->Substring(fragmentStart)
            : String::Empty;

        cleanedText = leadingWhitespace +
            candidate->Substring(0, queryStart) +
            queryPrefix +
            fragment +
            trailingWhitespace;
        return true;
    }

    bool TryCopyClipboardImageAsFile()
    {
        String^ imagePath = nullptr;
        try
        {
            if (!Clipboard::ContainsImage())
            {
                return false;
            }

            Image^ image = Clipboard::GetImage();
            if (image == nullptr)
            {
                throw gcnew InvalidOperationException(
                    "Windows reported an image on the clipboard but could not provide it.");
            }

            try
            {
                String^ tempDirectory = Environment::GetEnvironmentVariable("TEMP");
                if (String::IsNullOrEmpty(tempDirectory))
                {
                    tempDirectory = Environment::GetEnvironmentVariable("TMP");
                }
                if (String::IsNullOrEmpty(tempDirectory))
                {
                    throw gcnew InvalidOperationException(
                        "Windows did not provide a temporary directory.");
                }

                String^ imageDirectory = Path::Combine(
                    tempDirectory, "ClipBird");
                Directory::CreateDirectory(imageDirectory);
                imagePath = Path::Combine(
                    imageDirectory,
                    String::Format("Screenshot-{0}.png", Guid::NewGuid().ToString("N")));
                image->Save(imagePath, ImageFormat::Png);

                DataObject^ clipboardData = gcnew DataObject();
                clipboardData->SetData(DataFormats::Bitmap, true, image);
                clipboardData->SetData(
                    DataFormats::FileDrop,
                    true,
                    gcnew array<String^> { imagePath });
                Clipboard::SetDataObject(clipboardData, true);
            }
            finally
            {
                delete image;
            }

            String^ publishedImagePath = imagePath;
            temporaryScreenshotPaths->Add(imagePath);
            imagePath = nullptr;
            lastOwnedClipboardSequenceNumber = GetClipboardSequenceNumber();
            CleanupTemporaryScreenshots(publishedImagePath);
            trayIcon->Text = "ClipBird - image ready to paste as a file";
            return true;
        }
        catch (System::Runtime::InteropServices::ExternalException^ ex)
        {
            ShowTrayError(String::Format("Could not prepare clipboard image: {0}", ex->Message));
        }
        catch (IOException^ ex)
        {
            ShowTrayError(String::Format("Could not save clipboard image: {0}", ex->Message));
        }
        catch (UnauthorizedAccessException^ ex)
        {
            ShowTrayError(String::Format("Could not save clipboard image: {0}", ex->Message));
        }
        catch (InvalidOperationException^ ex)
        {
            ShowTrayError(String::Format("Could not prepare clipboard image: {0}", ex->Message));
        }
        finally
        {
            if (imagePath != nullptr && File::Exists(imagePath))
            {
                try
                {
                    File::Delete(imagePath);
                }
                catch (IOException^ ex)
                {
                    ShowTrayError(String::Format(
                        "Could not remove an incomplete clipboard image: {0}",
                        ex->Message));
                }
                catch (UnauthorizedAccessException^ ex)
                {
                    ShowTrayError(String::Format(
                        "Could not remove an incomplete clipboard image: {0}",
                        ex->Message));
                }
            }
        }

        return false;
    }

    void CleanupTemporaryScreenshots(String^ keepPath)
    {
        for (int index = temporaryScreenshotPaths->Count - 1; index >= 0; --index)
        {
            String^ path = temporaryScreenshotPaths[index];
            if (String::Equals(path, keepPath, StringComparison::OrdinalIgnoreCase))
            {
                continue;
            }

            try
            {
                File::Delete(path);
                temporaryScreenshotPaths->RemoveAt(index);
            }
            catch (IOException^ ex)
            {
                ShowTrayError(String::Format(
                    "Could not remove a previous clipboard image: {0}",
                    ex->Message));
            }
            catch (UnauthorizedAccessException^ ex)
            {
                ShowTrayError(String::Format(
                    "Could not remove a previous clipboard image: {0}",
                    ex->Message));
            }
        }
    }

    HGLOBAL CreateUnicodeClipboardData(String^ text)
    {
        SIZE_T characterCount = static_cast<SIZE_T>(text->Length) + 1;
        if (characterCount > static_cast<SIZE_T>(-1) / sizeof(wchar_t))
        {
            throw gcnew InvalidOperationException("The clipboard text is too large.");
        }

        SIZE_T bufferSize = characterCount * sizeof(wchar_t);
        HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bufferSize);
        if (data == nullptr)
        {
            DWORD errorCode = GetLastError();
            throw gcnew InvalidOperationException(String::Format(
                "Windows could not allocate plain clipboard text (error {0}).",
                errorCode));
        }

        bool keepData = false;
        try
        {
            void* lockedData = GlobalLock(data);
            if (lockedData == nullptr)
            {
                DWORD errorCode = GetLastError();
                throw gcnew InvalidOperationException(String::Format(
                    "Windows could not lock plain clipboard text (error {0}).",
                    errorCode));
            }

            try
            {
                IntPtr managedText =
                    System::Runtime::InteropServices::Marshal::StringToHGlobalUni(text);
                try
                {
                    std::memcpy(lockedData, managedText.ToPointer(), bufferSize);
                }
                finally
                {
                    System::Runtime::InteropServices::Marshal::FreeHGlobal(managedText);
                }
            }
            finally
            {
                GlobalUnlock(data);
            }

            keepData = true;
            return data;
        }
        finally
        {
            if (!keepData)
            {
                GlobalFree(data);
            }
        }
    }

    String^ ReadClipboardText(UINT format)
    {
        HANDLE hData = GetClipboardData(format);
        if (hData == nullptr)
        {
            DWORD errorCode = GetLastError();
            throw gcnew InvalidOperationException(String::Format(
                "Windows could not provide clipboard data (error {0}).",
                errorCode));
        }

        SIZE_T bufferSize = GlobalSize(hData);
        if (bufferSize == 0)
        {
            throw gcnew InvalidOperationException("The clipboard text buffer is empty or invalid.");
        }

        void* lockedData = GlobalLock(hData);
        if (lockedData == nullptr)
        {
            DWORD errorCode = GetLastError();
            throw gcnew InvalidOperationException(String::Format(
                "Windows could not lock clipboard data (error {0}).",
                errorCode));
        }

        try
        {
            if (format == CF_UNICODETEXT)
            {
                const wchar_t* text = static_cast<const wchar_t*>(lockedData);
                SIZE_T characterCount = bufferSize / sizeof(wchar_t);
                SIZE_T length = 0;
                while (length < characterCount && text[length] != L'\0')
                {
                    ++length;
                }

                if (length == characterCount || length > Int32::MaxValue)
                {
                    throw gcnew InvalidOperationException(
                        "The clipboard contains invalid or oversized Unicode text.");
                }

                return gcnew String(text, 0, static_cast<int>(length));
            }

            const char* text = static_cast<const char*>(lockedData);
            SIZE_T length = 0;
            while (length < bufferSize && text[length] != '\0')
            {
                ++length;
            }

            if (length == bufferSize || length > Int32::MaxValue)
            {
                throw gcnew InvalidOperationException(
                    "The clipboard contains invalid or oversized ANSI text.");
            }

            if (length == 0)
            {
                return String::Empty;
            }

            int sourceLength = static_cast<int>(length);
            int requiredLength = MultiByteToWideChar(
                CP_ACP, 0, text, sourceLength, nullptr, 0);
            if (requiredLength == 0)
            {
                DWORD errorCode = GetLastError();
                throw gcnew InvalidOperationException(String::Format(
                    "Windows could not convert clipboard text (error {0}).",
                    errorCode));
            }

            std::vector<wchar_t> convertedText(requiredLength);
            if (MultiByteToWideChar(
                    CP_ACP,
                    0,
                    text,
                    sourceLength,
                    convertedText.data(),
                    requiredLength) == 0)
            {
                DWORD errorCode = GetLastError();
                throw gcnew InvalidOperationException(String::Format(
                    "Windows could not convert clipboard text (error {0}).",
                    errorCode));
            }

            return gcnew String(convertedText.data(), 0, requiredLength);
        }
        finally
        {
            GlobalUnlock(hData);
        }
    }

    void ShowTrayError(String^ message)
    {
        if (trayIcon != nullptr && trayIcon->Visible)
        {
            trayIcon->BalloonTipTitle = "ClipBird";
            trayIcon->BalloonTipText = message;
            trayIcon->BalloonTipIcon = ToolTipIcon::Error;
            trayIcon->ShowBalloonTip(5000);
        }
    }
};