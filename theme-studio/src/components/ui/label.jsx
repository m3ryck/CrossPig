import * as LabelPrimitive from "@radix-ui/react-label";
import { cn } from "../../lib/utils";
export function Label({ className, ...props }) {
  return (
    <LabelPrimitive.Root className={cn("ui-label", className)} {...props} />
  );
}
