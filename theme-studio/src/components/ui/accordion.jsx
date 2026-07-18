import * as AccordionPrimitive from "@radix-ui/react-accordion";
import { ChevronDown } from "lucide-react";
import { cn } from "../../lib/utils";
export const Accordion = AccordionPrimitive.Root;
export function AccordionItem({ className, ...props }) {
  return (
    <AccordionPrimitive.Item
      className={cn("accordion-item", className)}
      {...props}
    />
  );
}
export function AccordionTrigger({ className, children, ...props }) {
  return (
    <AccordionPrimitive.Header>
      <AccordionPrimitive.Trigger
        className={cn("accordion-trigger", className)}
        {...props}
      >
        {children}
        <ChevronDown size={16} className="accordion-chevron" />
      </AccordionPrimitive.Trigger>
    </AccordionPrimitive.Header>
  );
}
export function AccordionContent({ className, children, ...props }) {
  return (
    <AccordionPrimitive.Content
      className={cn("accordion-content", className)}
      {...props}
    >
      <div className="accordion-inner">{children}</div>
    </AccordionPrimitive.Content>
  );
}
