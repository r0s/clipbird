#pragma once

using namespace System;

public ref class ClipboardTextProcessor abstract sealed
{
public:
    static bool Process(
        String^ text,
        bool convertToPlainText,
        bool removeUtmParameters,
        String^% processedText,
        bool% replacementRequired,
        bool% utmParametersRemoved)
    {
        processedText = text;
        replacementRequired = convertToPlainText;
        utmParametersRemoved = false;

        if (removeUtmParameters)
        {
            utmParametersRemoved =
                TryRemoveUtmParameters(text, processedText);
            replacementRequired =
                replacementRequired || utmParametersRemoved;
        }

        return replacementRequired;
    }

    static bool TryRemoveUtmParameters(
        String^ text,
        String^% cleanedText)
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
                parsedUri->Scheme,
                Uri::UriSchemeHttp,
                StringComparison::OrdinalIgnoreCase) &&
             !String::Equals(
                parsedUri->Scheme,
                Uri::UriSchemeHttps,
                StringComparison::OrdinalIgnoreCase)))
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
        String^ query = candidate->Substring(
            queryStart + 1, queryEnd - queryStart - 1);
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
            if (decodedName->StartsWith(
                "utm_", StringComparison::OrdinalIgnoreCase))
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

        int leadingWhitespaceLength =
            text->Length - text->TrimStart()->Length;
        int trailingWhitespaceLength =
            text->Length - text->TrimEnd()->Length;
        String^ leadingWhitespace =
            text->Substring(0, leadingWhitespaceLength);
        String^ trailingWhitespace = text->Substring(
            text->Length - trailingWhitespaceLength,
            trailingWhitespaceLength);
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
};
