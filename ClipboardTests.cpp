#include "ClipboardTextProcessor.h"

using namespace System;
using namespace System::Collections::Specialized;
using namespace System::Diagnostics;
using namespace System::Drawing;
using namespace System::IO;
using namespace System::Threading;
using namespace System::Windows::Forms;

static int testsRun = 0;

static void Assert(bool condition, String^ message)
{
    if (!condition)
    {
        throw gcnew InvalidOperationException(message);
    }
}

static void AssertUrlCase(
    String^ name,
    String^ input,
    bool expectedChanged,
    String^ expectedOutput)
{
    String^ actualOutput = nullptr;
    bool actualChanged =
        ClipboardTextProcessor::TryRemoveUtmParameters(input, actualOutput);

    Assert(
        actualChanged == expectedChanged,
        String::Format("{0}: expected changed={1}, got {2}.",
            name, expectedChanged, actualChanged));
    Assert(
        String::Equals(actualOutput, expectedOutput, StringComparison::Ordinal),
        String::Format("{0}: expected <{1}>, got <{2}>.",
            name, expectedOutput, actualOutput));
    ++testsRun;
    Console::WriteLine("PASS URL: {0}", name);
}

static void TestUrlCleaner()
{
    AssertUrlCase(
        "remove tracking keys anywhere and preserve other keys",
        "https://example.com/path?utm_source=x&keep=1&utm_medium=email#section",
        true,
        "https://example.com/path?keep=1#section");
    AssertUrlCase(
        "remove all tracking keys without leaving a question mark",
        "https://example.com/?utm_source=x&utm_campaign=y#section",
        true,
        "https://example.com/#section");
    AssertUrlCase(
        "match tracking key case insensitively",
        "https://example.com/?UTM_Source=x&keep=1",
        true,
        "https://example.com/?keep=1");
    AssertUrlCase(
        "decode encoded parameter names",
        "https://example.com/?%75tm_source=x&keep=1",
        true,
        "https://example.com/?keep=1");
    AssertUrlCase(
        "preserve empty and key-only nontracking parameters",
        "https://example.com/?&utm_source=x&keep",
        true,
        "https://example.com/?&keep");
    AssertUrlCase(
        "preserve value containing a tracking-like string",
        "https://example.com/?keep=utm_source",
        false,
        "https://example.com/?keep=utm_source");
    AssertUrlCase(
        "preserve URL without tracking parameters",
        "https://example.com/path?keep=1#section",
        false,
        "https://example.com/path?keep=1#section");
    AssertUrlCase(
        "reject multiline text",
        "https://example.com/?utm_source=x\nnot a URL",
        false,
        "https://example.com/?utm_source=x\nnot a URL");
    AssertUrlCase(
        "reject prose containing a URL",
        "Visit https://example.com/?utm_source=x",
        false,
        "Visit https://example.com/?utm_source=x");
    AssertUrlCase(
        "reject non-HTTP schemes",
        "mailto:user@example.com?utm_source=x",
        false,
        "mailto:user@example.com?utm_source=x");
    AssertUrlCase(
        "reject malformed URLs",
        "https://exa mple.com/?utm_source=x",
        false,
        "https://exa mple.com/?utm_source=x");
    AssertUrlCase(
        "preserve whitespace around a cleaned URL",
        "  https://example.com/?keep=1&utm_id=42  ",
        true,
        "  https://example.com/?keep=1  ");
}

static void AssertProcessingPolicy(
    String^ name,
    String^ input,
    bool convertToPlainText,
    bool removeUtm,
    bool expectedReplacement,
    bool expectedUtmRemoved,
    String^ expectedOutput)
{
    String^ actualOutput = nullptr;
    bool actualReplacement = false;
    bool actualUtmRemoved = false;
    bool returnedReplacement = ClipboardTextProcessor::Process(
        input,
        convertToPlainText,
        removeUtm,
        actualOutput,
        actualReplacement,
        actualUtmRemoved);

    Assert(returnedReplacement == expectedReplacement &&
        actualReplacement == expectedReplacement,
        String::Format("{0}: replacement policy mismatch.", name));
    Assert(actualUtmRemoved == expectedUtmRemoved,
        String::Format("{0}: UTM removal result mismatch.", name));
    Assert(String::Equals(actualOutput, expectedOutput, StringComparison::Ordinal),
        String::Format("{0}: expected <{1}>, got <{2}>.",
            name, expectedOutput, actualOutput));
    ++testsRun;
    Console::WriteLine("PASS policy: {0}", name);
}

static void TestProcessingPolicy()
{
    AssertProcessingPolicy(
        "UTM cleanup works without plaintext conversion",
        "https://example.com/?utm_source=x&keep=1",
        false,
        true,
        true,
        true,
        "https://example.com/?keep=1");
    AssertProcessingPolicy(
        "plaintext conversion works with URL cleanup disabled",
        "https://example.com/?utm_source=x",
        true,
        false,
        true,
        false,
        "https://example.com/?utm_source=x");
    AssertProcessingPolicy(
        "both disabled leaves text untouched",
        "https://example.com/?utm_source=x",
        false,
        false,
        false,
        false,
        "https://example.com/?utm_source=x");
    AssertProcessingPolicy(
        "plaintext conversion preserves ordinary prose",
        "plain text",
        true,
        true,
        true,
        false,
        "plain text");
}

static void SetClipboardObject(System::Windows::Forms::IDataObject^ data)
{
    Clipboard::SetDataObject(data, true);
}

static bool TryGetClipboardText(String^% text)
{
    try
    {
        if (!Clipboard::ContainsText(TextDataFormat::UnicodeText))
        {
            return false;
        }

        text = Clipboard::GetText(TextDataFormat::UnicodeText);
        return true;
    }
    catch (System::Runtime::InteropServices::ExternalException^)
    {
        return false;
    }
    catch (InvalidOperationException^)
    {
        return false;
    }
}

static void WaitForClipboardText(String^ expected, String^ caseName)
{
    for (int attempt = 0; attempt < 60; ++attempt)
    {
        Application::DoEvents();
        String^ actual = nullptr;
        if (TryGetClipboardText(actual) &&
            String::Equals(actual, expected, StringComparison::Ordinal))
        {
            ++testsRun;
            Console::WriteLine("PASS clipboard: {0}", caseName);
            return;
        }
        Thread::Sleep(100);
    }

    String^ lastValue = nullptr;
    TryGetClipboardText(lastValue);
    throw gcnew InvalidOperationException(String::Format(
        "{0}: expected clipboard text <{1}>, got <{2}>.",
        caseName, expected, lastValue));
}

static void WaitForFormatRemoval(String^ format, String^ caseName)
{
    for (int attempt = 0; attempt < 60; ++attempt)
    {
        Application::DoEvents();
        try
        {
            System::Windows::Forms::IDataObject^ data =
                Clipboard::GetDataObject();
            if (data == nullptr || !data->GetDataPresent(format, false))
            {
                ++testsRun;
                Console::WriteLine("PASS clipboard: {0}", caseName);
                return;
            }
        }
        catch (System::Runtime::InteropServices::ExternalException^)
        {
        }

        Thread::Sleep(100);
    }

    throw gcnew InvalidOperationException(
        caseName + ": clipboard format was not removed.");
}

static String^ WaitForImageFileDrop(String^ caseName)
{
    Thread::Sleep(250);
    for (int attempt = 0; attempt < 60; ++attempt)
    {
        Application::DoEvents();
        try
        {
            if (Clipboard::ContainsFileDropList())
            {
                StringCollection^ paths = Clipboard::GetFileDropList();
                if (paths != nullptr && paths->Count == 1 &&
                    File::Exists(paths[0]))
                {
                    ++testsRun;
                    Console::WriteLine("PASS image: {0}", caseName);
                    return paths[0];
                }
            }
        }
        catch (System::Runtime::InteropServices::ExternalException^)
        {
        }
        catch (InvalidOperationException^)
        {
        }

        Thread::Sleep(100);
    }

    throw gcnew InvalidOperationException(
        caseName + ": ClipBird did not publish a PNG file drop.");
}

static Bitmap^ SetClipboardImage()
{
    Bitmap^ bitmap = gcnew Bitmap(24, 24);
    try
    {
        Graphics^ graphics = Graphics::FromImage(bitmap);
        try
        {
            graphics->Clear(Color::CornflowerBlue);
        }
        finally
        {
            delete graphics;
        }

        DataObject^ clipboardData = gcnew DataObject();
        clipboardData->SetData(DataFormats::Bitmap, true, bitmap);
        Clipboard::SetDataObject(clipboardData, true);

        return bitmap;
    }
    catch (Exception^)
    {
        delete bitmap;
        throw;
    }
}

static void WaitForOldImageRemoval(String^ imagePath)
{
    for (int attempt = 0; attempt < 60; ++attempt)
    {
        bool fileDropStillPublished = false;
        try
        {
            if (Clipboard::ContainsFileDropList())
            {
                StringCollection^ paths = Clipboard::GetFileDropList();
                fileDropStillPublished =
                    paths != nullptr && paths->Contains(imagePath);
            }
        }
        catch (System::Runtime::InteropServices::ExternalException^)
        {
        }

        if (!fileDropStillPublished && !File::Exists(imagePath))
        {
            ++testsRun;
            Console::WriteLine(
                "PASS image: prior temporary PNG is removed after replacement");
            return;
        }
        Thread::Sleep(100);
    }

    throw gcnew InvalidOperationException(
        "The prior screenshot was not removed after clipboard replacement.");
}

static Process^ StartIsolatedClipBird(String^ workspace, String^ testDirectory)
{
    String^ appPath = Path::Combine(workspace, "ClipBird.exe");
    if (!File::Exists(appPath))
    {
        throw gcnew FileNotFoundException("Build ClipBird.exe before running tests.", appPath);
    }

    array<Process^>^ runningApps = Process::GetProcessesByName("ClipBird");
    if (runningApps->Length > 0)
    {
        for each (Process^ process in runningApps)
        {
            try
            {
                throw gcnew InvalidOperationException(String::Format(
                    "Close all ClipBird instances before running clipboard integration tests (found PID {0}).",
                    process->Id));
            }
            finally
            {
                delete process;
            }
        }
    }

    String^ isolatedAppPath = Path::Combine(testDirectory, "ClipBird.exe");
    File::Copy(appPath, isolatedAppPath);
    ProcessStartInfo^ startInfo = gcnew ProcessStartInfo();
    startInfo->FileName = isolatedAppPath;
    startInfo->WorkingDirectory = testDirectory;
    startInfo->UseShellExecute = false;
    Process^ app = Process::Start(startInfo);
    if (app == nullptr)
    {
        throw gcnew InvalidOperationException("Could not start the isolated ClipBird test instance.");
    }
    Thread::Sleep(1200);
    if (app->HasExited)
    {
        throw gcnew InvalidOperationException(String::Format(
            "The test ClipBird process exited with code {0}.", app->ExitCode));
    }
    return app;
}

static void TestClipboardIntegration(String^ workspace)
{
    System::Windows::Forms::IDataObject^ originalClipboard =
        Clipboard::GetDataObject();
    String^ testDirectory = Path::Combine(
        Environment::GetEnvironmentVariable("TEMP"),
        "ClipBird-Tests-" + Guid::NewGuid().ToString("N"));
    Directory::CreateDirectory(testDirectory);
    Process^ app = nullptr;
    String^ retainedImagePath = nullptr;

    try
    {
        app = StartIsolatedClipBird(workspace, testDirectory);

        SetClipboardObject(gcnew DataObject(
            DataFormats::UnicodeText,
            "https://example.com/article?keep=ok&utm_source=test#top"));
        WaitForClipboardText(
            "https://example.com/article?keep=ok#top",
            "single URL is cleaned by the running app");

        DataObject^ richText = gcnew DataObject();
        richText->SetData(
            DataFormats::UnicodeText,
            false,
            "clipboard plain text");
        richText->SetData(
            DataFormats::Rtf,
            false,
            "{\\rtf1 rich text}");
        SetClipboardObject(richText);
        WaitForClipboardText(
            "clipboard plain text",
            "ordinary text is converted to Unicode text");
        WaitForFormatRemoval(
            DataFormats::Rtf,
            "rich format is removed during plaintext conversion");

        SetClipboardObject(gcnew DataObject(
            DataFormats::Text,
            "ANSI fallback text"));
        WaitForClipboardText(
            "ANSI fallback text",
            "ANSI clipboard text is converted to Unicode");

        Clipboard::Clear();
        Thread::Sleep(300);
        Assert(
            !Clipboard::ContainsText() &&
            !Clipboard::ContainsImage() &&
            !Clipboard::ContainsFileDropList(),
            "An empty clipboard was unexpectedly populated by ClipBird.");
        ++testsRun;
        Console::WriteLine("PASS clipboard: empty clipboard remains empty");

        Bitmap^ firstBitmap = SetClipboardImage();
        Assert(Clipboard::ContainsImage(),
            "The native bitmap test data is not recognized as an image.");
        String^ firstImagePath = WaitForImageFileDrop(
            "screenshot provides bitmap and file-drop formats");
        delete firstBitmap;
        Assert(Clipboard::ContainsImage(),
            "Image was not available after ClipBird processed the screenshot.");
        System::Windows::Forms::IDataObject^ imageClipboard =
            Clipboard::GetDataObject();
        Assert(imageClipboard->GetDataPresent(DataFormats::Bitmap, false),
            "Bitmap format was not retained on the image clipboard.");
        ++testsRun;
        Console::WriteLine("PASS image: bitmap format is retained");

        SetClipboardObject(gcnew DataObject(
            DataFormats::UnicodeText,
            "replace screenshot with text"));
        WaitForClipboardText(
            "replace screenshot with text",
            "text replaces screenshot clipboard");
        WaitForOldImageRemoval(firstImagePath);

        Bitmap^ secondBitmap = SetClipboardImage();
        retainedImagePath = WaitForImageFileDrop(
            "second screenshot publishes a PNG");
        delete secondBitmap;
        app->Kill();
        app->WaitForExit();
        if (!File::Exists(retainedImagePath))
        {
            throw gcnew InvalidOperationException(
                "ClipBird removed a PNG that was still available on the clipboard at exit.");
        }
        ++testsRun;
        Console::WriteLine("PASS image: published PNG is retained at app exit");
    }
    finally
    {
        if (app != nullptr)
        {
            if (!app->HasExited)
            {
                app->Kill();
                app->WaitForExit();
            }
            delete app;
        }

        if (originalClipboard != nullptr)
        {
            Clipboard::SetDataObject(originalClipboard, true);
        }
        else
        {
            Clipboard::Clear();
        }
        if (!String::IsNullOrEmpty(retainedImagePath) &&
            File::Exists(retainedImagePath))
        {
            File::Delete(retainedImagePath);
        }
        if (Directory::Exists(testDirectory))
        {
            Directory::Delete(testDirectory, true);
        }
    }
}

[STAThread]
int main(array<String^>^ args)
{
    try
    {
        String^ workspace = AppDomain::CurrentDomain->BaseDirectory;
        TestUrlCleaner();
        TestProcessingPolicy();
        TestClipboardIntegration(workspace);
        Console::WriteLine(
            "Passed {0} ClipBird tests.", testsRun);
        return 0;
    }
    catch (Exception^ ex)
    {
        Console::Error->WriteLine(
            "ClipBird tests failed after {0} checks: {1}",
            testsRun,
            ex);
        return 1;
    }
}
