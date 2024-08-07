#pragma once
namespace dspar
{
    struct DSParNodeConfiguration
    {
        bool AskForDemandUpstream;
        bool WaitForDemandDownstream;
        bool Ordered;

        DSParNodeConfiguration()
        {
            AskForDemandUpstream = false;
            WaitForDemandDownstream = false;
            Ordered = false;
        }
    };
} // namespace dspar