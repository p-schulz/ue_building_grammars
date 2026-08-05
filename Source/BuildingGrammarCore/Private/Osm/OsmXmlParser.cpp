#include "Osm/OsmTypes.h"
#include "XmlFile.h"
#include "XmlNode.h"

namespace
{
	void ParseTagChildren(const FXmlNode* Parent, TMap<FString, FString>& OutTags)
	{
		for (const FXmlNode* Child = Parent->GetFirstChildNode(); Child; Child = Child->GetNextNode())
		{
			if (Child->GetTag() == TEXT("tag"))
			{
				const FString Key = Child->GetAttribute(TEXT("k"));
				if (!Key.IsEmpty())
				{
					OutTags.Add(Key, Child->GetAttribute(TEXT("v")));
				}
			}
		}
	}
}

bool FOsmDocument::ParseFile(const FString& FilePath, FOsmDocument& OutDocument, FString& OutError)
{
	FXmlFile XmlFile;
	if (!XmlFile.LoadFile(FilePath))
	{
		OutError = XmlFile.GetLastError();
		return false;
	}

	const FXmlNode* Root = XmlFile.GetRootNode();
	if (!Root)
	{
		OutError = TEXT("OSM XML file has no root node");
		return false;
	}

	OutDocument = FOsmDocument();

	for (const FXmlNode* Child = Root->GetFirstChildNode(); Child; Child = Child->GetNextNode())
	{
		const FString& Tag = Child->GetTag();

		if (Tag == TEXT("node"))
		{
			FOsmNode Node;
			Node.Id = FCString::Atoi64(*Child->GetAttribute(TEXT("id")));
			Node.Lat = FCString::Atod(*Child->GetAttribute(TEXT("lat")));
			Node.Lon = FCString::Atod(*Child->GetAttribute(TEXT("lon")));
			OutDocument.Nodes.Add(Node.Id, Node);
		}
		else if (Tag == TEXT("way"))
		{
			FOsmWay Way;
			Way.Id = FCString::Atoi64(*Child->GetAttribute(TEXT("id")));
			for (const FXmlNode* WayChild = Child->GetFirstChildNode(); WayChild; WayChild = WayChild->GetNextNode())
			{
				if (WayChild->GetTag() == TEXT("nd"))
				{
					const FString Ref = WayChild->GetAttribute(TEXT("ref"));
					if (!Ref.IsEmpty())
					{
						Way.NodeRefs.Add(FCString::Atoi64(*Ref));
					}
				}
			}
			ParseTagChildren(Child, Way.Tags);
			OutDocument.Ways.Add(Way.Id, Way);
		}
		else if (Tag == TEXT("relation"))
		{
			FOsmRelation Relation;
			Relation.Id = FCString::Atoi64(*Child->GetAttribute(TEXT("id")));
			for (const FXmlNode* RelChild = Child->GetFirstChildNode(); RelChild; RelChild = RelChild->GetNextNode())
			{
				if (RelChild->GetTag() == TEXT("member"))
				{
					FOsmRelationMember Member;
					Member.Type = RelChild->GetAttribute(TEXT("type"));
					Member.Ref = FCString::Atoi64(*RelChild->GetAttribute(TEXT("ref")));
					Member.Role = RelChild->GetAttribute(TEXT("role"));
					Relation.Members.Add(Member);
				}
			}
			ParseTagChildren(Child, Relation.Tags);
			OutDocument.Relations.Add(Relation.Id, Relation);
		}
	}

	return true;
}

bool FOsmDocument::ComputeBoundsCenter(double& OutCenterLatitude, double& OutCenterLongitude) const
{
	if (Nodes.Num() == 0)
	{
		return false;
	}
	double MinLat = TNumericLimits<double>::Max();
	double MaxLat = TNumericLimits<double>::Lowest();
	double MinLon = TNumericLimits<double>::Max();
	double MaxLon = TNumericLimits<double>::Lowest();
	for (const TPair<int64, FOsmNode>& NodePair : Nodes)
	{
		MinLat = FMath::Min(MinLat, NodePair.Value.Lat);
		MaxLat = FMath::Max(MaxLat, NodePair.Value.Lat);
		MinLon = FMath::Min(MinLon, NodePair.Value.Lon);
		MaxLon = FMath::Max(MaxLon, NodePair.Value.Lon);
	}
	OutCenterLatitude = (MinLat + MaxLat) / 2.0;
	OutCenterLongitude = (MinLon + MaxLon) / 2.0;
	return true;
}
